#include "durability.hpp"

#include <algorithm>

namespace durability {

namespace {

/// Measured power on step (i-1 -> i): the mean of whichever endpoints carry it.
Real measured_step(const std::vector<TrackPoint>& pts, Size i) {
    const Bool a = pts[i - 1].has_power, b = pts[i].has_power;
    if (a && b) return 0.5 * (pts[i - 1].power + pts[i].power);
    if (b)      return static_cast<Real>(pts[i].power);
    if (a)      return static_cast<Real>(pts[i - 1].power);
    return 0.0;
}

/// The ride reduced to what a best-effort search needs: cumulative valid time
/// and cumulative energy, indexed by point, both zero at the start.
struct Cumulative {
    std::vector<Real> t_s;   ///< Seconds of riding up to each point.
    std::vector<Real> j;     ///< Joules of work up to each point.
};

Cumulative accumulate(const std::vector<TrackPoint>& pts, const PowerAnalysis& pa,
                      Bool measured) {
    const Size n = pts.size();
    Cumulative c;
    c.t_s.assign(n, 0.0);
    c.j.assign(n, 0.0);

    for (Size i = 1; i < n; ++i) {
        Real dt = 0.0;
        if (pa.t_offset_s[i] >= 0 && pa.t_offset_s[i - 1] >= 0) {
            const Long d = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
            if (d > 0) dt = static_cast<Real>(d);
        }
        const Real p = measured ? measured_step(pts, i) : pa.point_power_w[i];
        c.t_s[i] = c.t_s[i - 1] + dt;
        c.j[i]   = c.j[i - 1] + p * dt;
    }
    return c;
}

constexpr Real kJoulesPerKj = 1000.0;

/// Best average power over a window of at least `duration_s` that begins at or
/// after `from_index`.
///
/// The same two-pointer sweep peaks::best_efforts uses, with the start bounded
/// below. Windows are "at least" the duration rather than exactly it because
/// the samples are irregular; the hi pointer never rewinds, so each start gets
/// the shortest window that qualifies.
Effort best_from(const Cumulative& c, Size from_index, Size n, Long duration_s,
                 const PowerAnalysis& pa) {
    Effort e;
    e.duration_s = duration_s;

    const Real want = static_cast<Real>(duration_s);
    Size       hi   = from_index + 1;

    for (Size lo = from_index; lo + 1 < n; ++lo) {
        if (hi <= lo) hi = lo + 1;
        while (hi < n - 1 && (c.t_s[hi] - c.t_s[lo]) < want) ++hi;

        const Real span = c.t_s[hi] - c.t_s[lo];
        if (span < want) continue;

        const Real avg = (c.j[hi] - c.j[lo]) / span;
        if (!e.found || avg > e.avg_power_w) {
            e.avg_power_w    = avg;
            e.start_offset_s = (pa.t_offset_s[lo] >= 0) ? pa.t_offset_s[lo] : 0;
            e.found          = true;
        }
    }
    return e;
}

/// First point at which `kj` of work has been done.
Size index_at_work(const Cumulative& c, Real kj) {
    const Real target = kj * kJoulesPerKj;
    const auto it = std::lower_bound(c.j.begin(), c.j.end(), target);
    return static_cast<Size>(it - c.j.begin());
}

} // namespace

Report analyse(const Track& track, const PowerAnalysis& pa,
               const std::vector<Long>& durations_s,
               const std::vector<Real>& thresholds_kj) {
    Report r;

    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (n < 2 || !pa.stats.valid ||
        pa.point_power_w.size() != n || pa.t_offset_s.size() != n)
        return r;

    r.measured = pa.stats.has_measured;

    const Cumulative c = accumulate(pts, pa, r.measured);
    r.total_kj = c.j.back() / kJoulesPerKj;
    if (!(r.total_kj > 0.0) || !(c.t_s.back() > 0.0)) return r;

    // A threshold the ride never reached would report nothing useful, so the
    // table simply stops there rather than carrying empty columns.
    for (const Real kj : thresholds_kj)
        if (kj < r.total_kj) r.thresholds_kj.push_back(kj);
    if (r.thresholds_kj.size() < 2) return r;

    for (const Long d : durations_s) {
        if (d <= 0 || static_cast<Real>(d) > c.t_s.back()) continue;

        Curve curve;
        curve.duration_s = d;
        for (const Real kj : r.thresholds_kj) {
            Effort e = best_from(c, index_at_work(c, kj), n, d, pa);
            e.after_kj = kj;
            curve.efforts.push_back(e);
        }

        // The fade compares the freshest reading with the deepest one that was
        // actually reached, so a ride that ends mid-table still reports a
        // meaningful drop rather than none.
        const auto first = std::find_if(curve.efforts.begin(), curve.efforts.end(),
                                        [](const Effort& e) { return e.found; });
        const auto last  = std::find_if(curve.efforts.rbegin(), curve.efforts.rend(),
                                        [](const Effort& e) { return e.found; });
        if (first != curve.efforts.end() && last != curve.efforts.rend() &&
            &*first != &*last && first->avg_power_w > 0.0) {
            curve.fade_pct =
                (first->avg_power_w - last->avg_power_w) / first->avg_power_w * 100.0;
            curve.valid = true;
        }
        r.curves.push_back(std::move(curve));
    }

    r.valid = std::any_of(r.curves.begin(), r.curves.end(),
                          [](const Curve& c2) { return c2.valid; });
    return r;
}

} // namespace durability
