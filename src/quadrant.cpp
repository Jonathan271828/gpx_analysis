#include "quadrant.hpp"

#include "ride.hpp"

#include <cmath>
#include <vector>

namespace quadrant {

namespace {

constexpr Real REF_CADENCE = 90.0; // crosshair reference cadence (rpm)

/// Circumferential pedal velocity (m/s) for a cadence in rpm.
Real cpv(Real cadence_rpm, Real crank_m) {
    return cadence_rpm / 60.0 * 2.0 * M_PI * crank_m;
}

} // namespace

Quadrants analyse(const Track& track, const PowerAnalysis& pa,
                  Real ftp_w, Real crank_length_m) {
    Quadrants q;
    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (!pa.stats.valid || ftp_w <= 0.0 || crank_length_m <= 0.0) return q;
    if (pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return q;

    // Crosshair: pedal velocity and force corresponding to FTP at 90 rpm.
    q.cpv_threshold_ms = cpv(REF_CADENCE, crank_length_m);
    if (q.cpv_threshold_ms <= 0.0) return q;
    q.aepf_threshold_n = ftp_w / q.cpv_threshold_ms;

    const Bool measured = pa.stats.has_measured;
    double cad_sum = 0.0, cad_w = 0.0;

    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > ride::kStopSeconds) continue;
        if (!pts[i].has_cad || pts[i].cad <= 0) continue;   // coasting / no data

        const Real v = cpv(static_cast<Real>(pts[i].cad), crank_length_m);
        if (v <= 0.0) continue;
        const Real p = measured ? ride::step_power(pts, pa, i, true) : pa.point_power_w[i];
        const Real f = p / v;                                // AEPF (N)
        const Real w = static_cast<Real>(dt);

        const Bool hi_f = f >= q.aepf_threshold_n;
        const Bool hi_v = v >= q.cpv_threshold_ms;
        const int  idx  = hi_f ? (hi_v ? 0 : 1) : (hi_v ? 3 : 2);
        q.seconds[idx] += w;
        q.total_s      += w;
        cad_sum += static_cast<double>(pts[i].cad) * w;
        cad_w   += w;
    }

    if (cad_w > 0.0) q.avg_cadence_rpm = static_cast<Real>(cad_sum / cad_w);
    q.valid = q.total_s > 0.0;
    return q;
}

} // namespace quadrant
