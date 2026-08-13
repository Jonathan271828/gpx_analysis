#include "durability.hpp"

#include "ride.hpp"

#include <algorithm>

namespace durability {

namespace {

constexpr Real kJoulesPerKj = 1000.0;

/// First point at which `kj` of work has been done.
Size index_at_work(const ride::Cumulative& c, Real kj) {
    const Real target = kj * kJoulesPerKj;
    const auto it = std::lower_bound(c.joules.begin(), c.joules.end(), target);
    return static_cast<Size>(it - c.joules.begin());
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

    const ride::Cumulative c = ride::accumulate(track, pa, r.measured);
    r.total_kj = c.total_joules() / kJoulesPerKj;
    if (!(r.total_kj > 0.0) || !(c.total_seconds() > 0.0)) return r;

    // A threshold the ride never reached would report nothing useful, so the
    // table simply stops there rather than carrying empty columns.
    for (const Real kj : thresholds_kj)
        if (kj < r.total_kj) r.thresholds_kj.push_back(kj);
    if (r.thresholds_kj.size() < 2) return r;

    for (const Long d : durations_s) {
        if (d <= 0 || static_cast<Real>(d) > c.total_seconds()) continue;

        Curve curve;
        curve.duration_s = d;
        for (const Real kj : r.thresholds_kj) {
            const ride::Window w = ride::best_window(c, index_at_work(c, kj), d);

            Effort e;
            e.duration_s     = d;
            e.after_kj       = kj;
            e.found          = w.found;
            e.avg_power_w    = w.mean_power_w;
            e.start_offset_s = w.found && pa.t_offset_s[w.begin] >= 0
                                   ? pa.t_offset_s[w.begin] : 0;
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
