#include "ride.hpp"

namespace ride {

Real step_seconds(const PowerAnalysis& pa, Size i) {
    if (i == 0 || i >= pa.t_offset_s.size()) return 0.0;
    if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) return 0.0;

    const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
    return dt > 0 ? static_cast<Real>(dt) : 0.0;
}

Bool is_riding(const PowerAnalysis& pa, Size i) {
    const Real dt = step_seconds(pa, i);
    return dt > 0.0 && dt <= static_cast<Real>(kStopSeconds);
}

Real step_power(const std::vector<TrackPoint>& pts, const PowerAnalysis& pa,
                Size i, Bool prefer_measured) {
    if (!prefer_measured)
        return (i < pa.point_power_w.size()) ? pa.point_power_w[i] : 0.0;

    if (i == 0 || i >= pts.size()) return 0.0;
    const Bool before = pts[i - 1].has_power;
    const Bool after  = pts[i].has_power;

    if (before && after) return 0.5 * (pts[i - 1].power + pts[i].power);
    if (after)           return static_cast<Real>(pts[i].power);
    if (before)          return static_cast<Real>(pts[i - 1].power);
    return 0.0;
}

Cumulative accumulate(const Track& track, const PowerAnalysis& pa,
                      Bool prefer_measured) {
    const std::vector<TrackPoint>& pts = track.points;
    const Size                     n   = pts.size();

    Cumulative c;
    c.seconds.assign(n, 0.0);
    c.joules.assign(n, 0.0);

    for (Size i = 1; i < n; ++i) {
        const Real dt = step_seconds(pa, i);
        const Real p  = step_power(pts, pa, i, prefer_measured);
        c.seconds[i]  = c.seconds[i - 1] + dt;
        c.joules[i]   = c.joules[i - 1] + p * dt;
    }
    return c;
}

Window best_window(const Cumulative& c, Size from, Long duration_s) {
    Window w;
    const Size n = c.seconds.size();
    if (n < 2 || duration_s <= 0) return w;

    const Real want = static_cast<Real>(duration_s);
    Size       hi   = from + 1;

    for (Size lo = from; lo + 1 < n; ++lo) {
        if (hi <= lo) hi = lo + 1;
        while (hi < n - 1 && (c.seconds[hi] - c.seconds[lo]) < want) ++hi;

        const Real span = c.seconds[hi] - c.seconds[lo];
        if (span < want) continue;

        const Real mean = (c.joules[hi] - c.joules[lo]) / span;
        if (!w.found || mean > w.mean_power_w) {
            w.mean_power_w = mean;
            w.begin        = lo;
            w.end          = hi;
            w.found        = true;
        }
    }
    return w;
}

Real mean_power_over(const Cumulative& c, const Window& w) {
    if (!w.found || w.end >= c.seconds.size()) return 0.0;
    const Real span = c.seconds[w.end] - c.seconds[w.begin];
    if (!(span > 0.0)) return 0.0;
    return (c.joules[w.end] - c.joules[w.begin]) / span;
}

} // namespace ride
