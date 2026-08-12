#include "peaks.hpp"

#include <vector>

namespace peaks {

namespace {

/// Measured power on step (i-1 -> i): mean of the endpoints that carry <power>.
Real measured_step(const std::vector<TrackPoint>& pts, Size i) {
    const Bool a = pts[i - 1].has_power, b = pts[i].has_power;
    if (a && b) return 0.5 * (pts[i - 1].power + pts[i].power);
    if (b)      return static_cast<Real>(pts[i].power);
    if (a)      return static_cast<Real>(pts[i - 1].power);
    return 0.0;
}

} // namespace

std::vector<PeakEffort> best_efforts(const Track& track, const PowerAnalysis& pa,
                                     const std::vector<Long>& durations_s) {
    std::vector<PeakEffort> out;
    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (n < 2 || !pa.stats.valid ||
        pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return out;

    const Bool measured = pa.stats.has_measured;

    // Cumulative valid time and energy (index 0 == 0); gaps contribute nothing.
    std::vector<Real> ct(n, 0.0), ce(n, 0.0);
    for (Size i = 1; i < n; ++i) {
        Real dt = 0.0;
        if (pa.t_offset_s[i] >= 0 && pa.t_offset_s[i - 1] >= 0) {
            const Long d = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
            if (d > 0) dt = static_cast<Real>(d);
        }
        const Real p = measured ? measured_step(pts, i) : pa.point_power_w[i];
        ct[i] = ct[i - 1] + dt;
        ce[i] = ce[i - 1] + p * dt;
    }
    const Real total = ct.back();
    if (total <= 0.0) return out;

    for (Long D : durations_s) {
        if (D <= 0 || static_cast<Real>(D) > total) continue;

        // Two-pointer sliding window maximising energy/time over windows of at
        // least D seconds (mirrors the mean-maximal power-curve search).
        Real  best  = 0.0;
        Size  best_lo = 0;
        Bool  found = false;
        Size  hi = 1;
        for (Size lo = 0; lo < n - 1; ++lo) {
            while (hi < n - 1 && (ct[hi] - ct[lo]) < static_cast<Real>(D)) ++hi;
            const Real span = ct[hi] - ct[lo];
            if (span < static_cast<Real>(D)) continue;
            const Real avg = (ce[hi] - ce[lo]) / span;
            if (!found || avg > best) { best = avg; best_lo = lo; found = true; }
        }
        if (!found) continue;

        PeakEffort pe;
        pe.duration_s     = D;
        pe.avg_power_w     = best;
        pe.start_offset_s  = (pa.t_offset_s[best_lo] >= 0) ? pa.t_offset_s[best_lo] : 0;
        pe.start_time      = pts[best_lo].time;
        pe.measured        = measured;
        out.push_back(pe);
    }
    return out;
}

} // namespace peaks
