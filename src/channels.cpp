#include "channels.hpp"

#include <cmath>
#include <utility>

namespace channels {

std::vector<Channel> extract(const Track& track, const TrackStats& stats,
                             const PowerAnalysis& pa) {
    const auto& pts = track.points;
    const Size  n   = pts.size();

    Channel velocity{"velocity", "km/h", {}, {}};
    Channel est_power{"power", "W", {}, {}};
    Channel meas_power{"power_measured", "W", {}, {}};
    Channel hr{"hr", "bpm", {}, {}};
    Channel cadence{"cadence", "rpm", {}, {}};
    Channel torque{"torque", "Nm", {}, {}};

    // Torque is power over angular velocity, so it needs both, and it needs the
    // cranks to be turning: at a handful of rpm the quotient explodes into
    // hundreds of newton-metres that describe a sensor artefact rather than a
    // rider. Below this the sample is dropped rather than clamped, because a
    // clamped value would still be plotted as if it meant something.
    constexpr Int  kMinCadenceRpm = 20;
    constexpr Real kRadPerRevMin  = 2.0 * M_PI / 60.0;

    // Torque follows whichever power series the rest of the analysis trusts.
    const Bool measured = stats.has_power;

    for (Size i = 0; i < n; ++i) {
        const Long t = (i < pa.t_offset_s.size()) ? pa.t_offset_s[i] : -1;
        if (t < 0) continue;
        const Real ts = static_cast<Real>(t);

        // Velocity and estimated power are step quantities (defined for i >= 1).
        if (i > 0 && pa.stats.valid) {
            velocity.t_s.push_back(ts);
            velocity.value.push_back(pa.speed_ms[i] * 3.6);   // m/s -> km/h
            est_power.t_s.push_back(ts);
            est_power.value.push_back(pa.point_power_w[i]);
        }
        // Sensor channels: only where the point actually carries the field.
        if (pts[i].has_power) {
            meas_power.t_s.push_back(ts);
            meas_power.value.push_back(static_cast<Real>(pts[i].power));
        }
        if (pts[i].has_hr) {
            hr.t_s.push_back(ts);
            hr.value.push_back(static_cast<Real>(pts[i].hr));
        }
        if (pts[i].has_cad) {
            cadence.t_s.push_back(ts);
            cadence.value.push_back(static_cast<Real>(pts[i].cad));
        }
        if (i > 0 && pa.stats.valid && pts[i].has_cad &&
            pts[i].cad >= kMinCadenceRpm) {
            const Real watts = (measured && pts[i].has_power)
                                   ? static_cast<Real>(pts[i].power)
                                   : pa.point_power_w[i];
            const Real omega = static_cast<Real>(pts[i].cad) * kRadPerRevMin;
            torque.t_s.push_back(ts);
            torque.value.push_back(watts / omega);
        }
    }

    std::vector<Channel> out;
    if (pa.stats.valid) {
        out.push_back(std::move(velocity));
        out.push_back(std::move(est_power));
    }
    if (stats.has_power) out.push_back(std::move(meas_power));
    if (stats.has_hr)    out.push_back(std::move(hr));
    if (stats.has_cad)   out.push_back(std::move(cadence));
    if (stats.has_cad && pa.stats.valid && torque.t_s.size() >= 2)
        out.push_back(std::move(torque));
    return out;
}

} // namespace channels
