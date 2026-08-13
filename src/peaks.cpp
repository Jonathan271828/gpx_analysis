#include "peaks.hpp"

#include "ride.hpp"

#include <vector>

namespace peaks {

namespace {

} // namespace

std::vector<PeakEffort> best_efforts(const Track& track, const PowerAnalysis& pa,
                                     const std::vector<Long>& durations_s) {
    std::vector<PeakEffort> out;
    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (n < 2 || !pa.stats.valid ||
        pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return out;

    const Bool             measured = pa.stats.has_measured;
    const ride::Cumulative c        = ride::accumulate(track, pa, measured);
    const Real             total    = c.total_seconds();
    if (total <= 0.0) return out;

    for (Long D : durations_s) {
        if (D <= 0 || static_cast<Real>(D) > total) continue;

        const ride::Window w = ride::best_window(c, 0, D);
        if (!w.found) continue;

        PeakEffort pe;
        pe.duration_s     = D;
        pe.avg_power_w    = w.mean_power_w;
        pe.start_offset_s = (pa.t_offset_s[w.begin] >= 0) ? pa.t_offset_s[w.begin] : 0;
        pe.start_time     = pts[w.begin].time;
        pe.measured       = measured;
        out.push_back(pe);
    }
    return out;
}

} // namespace peaks
