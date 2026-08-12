#include "splits.hpp"

#include <cmath>
#include <vector>

namespace splits {

std::vector<Split> by_distance(const Track& track, const PowerAnalysis& pa,
                               Real split_km) {
    std::vector<Split> out;
    if (split_km <= 0.0) return out;

    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (n < 2 || pa.cum_dist_m.size() != n || pa.t_offset_s.size() != n) return out;

    const Real seg   = split_km * 1000.0;
    const Real total = pa.cum_dist_m.back();
    if (total <= 0.0) return out;

    const Size nsplits = static_cast<Size>(std::floor(total / seg)) + 1;
    out.resize(nsplits);
    std::vector<Real> pw_sum(nsplits, 0), pw_w(nsplits, 0);
    std::vector<Real> hr_sum(nsplits, 0), hr_w(nsplits, 0);

    for (Size k = 0; k < nsplits; ++k) {
        out[k].start_km = static_cast<Real>(k) * split_km;
        out[k].end_km   = static_cast<Real>(k + 1) * split_km;
    }

    for (Size i = 1; i < n; ++i) {
        Size k = static_cast<Size>(std::floor(pa.cum_dist_m[i] / seg));
        if (k >= nsplits) k = nsplits - 1;

        out[k].dist_m += pa.cum_dist_m[i] - pa.cum_dist_m[i - 1];

        const Real de = pts[i].ele - pts[i - 1].ele;
        if (de > 0.0) out[k].gain_m += de; else out[k].loss_m += -de;

        if (pa.t_offset_s[i] >= 0 && pa.t_offset_s[i - 1] >= 0) {
            const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
            if (dt > 0) {
                out[k].dur_s += dt;
                if (pa.stats.valid) {
                    pw_sum[k] += pa.point_power_w[i] * static_cast<Real>(dt);
                    pw_w[k]   += static_cast<Real>(dt);
                }
                if (pts[i].has_hr) {
                    hr_sum[k] += static_cast<Real>(pts[i].hr) * static_cast<Real>(dt);
                    hr_w[k]   += static_cast<Real>(dt);
                }
            }
        }
    }

    for (Size k = 0; k < nsplits; ++k) {
        Split& sp = out[k];
        if (sp.dur_s > 0)
            sp.avg_speed_kmh = (sp.dist_m / 1000.0) / (static_cast<Real>(sp.dur_s) / 3600.0);
        if (pw_w[k] > 0.0) { sp.has_power = true; sp.avg_power_w = pw_sum[k] / pw_w[k]; }
        if (hr_w[k] > 0.0) { sp.has_hr    = true; sp.avg_hr      = hr_sum[k] / hr_w[k]; }
    }
    return out;
}

} // namespace splits
