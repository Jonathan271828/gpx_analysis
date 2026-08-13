#include "metrics.hpp"

#include "ride.hpp"

#include <cmath>
#include <vector>

namespace metrics {

namespace {

// Steps longer than this are treated as a stop and dropped from moving time.

/// Measured power on step (i-1 -> i): mean of the endpoints that carry <power>.
/// Moving-time power series at 1 Hz (stops removed), by holding each step's
/// power for its whole duration. Also returns the total mechanical work (J).
std::vector<Real> moving_power_1hz(const Track& track, const PowerAnalysis& pa,
                                   Real& energy_j, Bool& measured) {
    std::vector<Real> g;
    energy_j = 0.0;
    const auto& pts = track.points;
    const Size  n   = pts.size();
    measured = pa.stats.has_measured;
    if (pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return g;

    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long d = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (d <= 0 || d > ride::kStopSeconds) continue;               // gap / stop
        const Real pw = measured ? ride::step_power(pts, pa, i, true) : pa.point_power_w[i];
        energy_j += pw * static_cast<Real>(d);
        for (Long k = 0; k < d; ++k) g.push_back(pw);
    }
    return g;
}

} // namespace

// ---------------------------------------------------------------------------
// training_load
// ---------------------------------------------------------------------------

TrainingLoad training_load(const Track& track, const PowerAnalysis& pa,
                           Real ftp_w, Real body_mass_kg) {
    TrainingLoad m;
    if (!pa.stats.valid || ftp_w <= 0.0) return m;

    Real energy_j = 0.0;
    Bool measured = false;
    const std::vector<Real> g = moving_power_1hz(track, pa, energy_j, measured);
    if (g.size() < 30) return m;                          // too little to be useful

    // Average power over moving time.
    double sum = 0.0;
    for (Real p : g) sum += p;
    const Real avg = static_cast<Real>(sum / g.size());

    // Normalized Power: 30 s rolling average, then fourth-root of the mean of
    // the fourth powers.
    double run = 0.0, s4 = 0.0;
    for (Size k = 0; k < g.size(); ++k) {
        run += g[k];
        if (k >= 30) run -= g[k - 30];
        const Size win = (k < 30) ? (k + 1) : 30;
        const double roll = run / static_cast<double>(win);
        s4 += roll * roll * roll * roll;
    }
    const Real np = static_cast<Real>(std::pow(s4 / static_cast<double>(g.size()), 0.25));

    m.valid         = true;
    m.from_measured = measured;
    m.avg_power_w   = avg;
    m.np_w          = np;
    m.ftp_w         = ftp_w;
    m.if_           = np / ftp_w;
    m.vi            = (avg > 0.0) ? np / avg : 0.0;
    m.moving_s      = static_cast<Long>(g.size());
    m.tss           = (static_cast<double>(m.moving_s) * np * m.if_)
                    / (ftp_w * 3600.0) * 100.0;
    m.kj            = energy_j / 1000.0;
    // For cycling, mechanical kJ ~= dietary kcal: human efficiency ~24% and
    // 1 kcal = 4.184 kJ nearly cancel, so we report kcal = kJ.
    m.kcal          = m.kj;
    m.body_mass_kg  = body_mass_kg;
    if (body_mass_kg > 0.0) {
        m.avg_wkg = avg / body_mass_kg;
        m.np_wkg  = np  / body_mass_kg;
    }

    // Efficiency Factor = NP / average heart rate over moving time.
    const auto& pts = track.points;
    double hr_sum = 0.0, hr_w = 0.0;
    for (Size i = 1; i < pts.size(); ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > ride::kStopSeconds || !pts[i].has_hr) continue;
        hr_sum += static_cast<double>(pts[i].hr) * dt;
        hr_w   += dt;
    }
    if (hr_w > 0.0) {
        m.has_hr = true;
        m.avg_hr = static_cast<Real>(hr_sum / hr_w);
        if (m.avg_hr > 0.0) m.ef = np / m.avg_hr;
    }
    return m;
}

// ---------------------------------------------------------------------------
// decoupling
// ---------------------------------------------------------------------------

Decoupling decoupling(const Track& track, const PowerAnalysis& pa) {
    Decoupling d;
    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (!pa.stats.valid || pa.t_offset_s.size() != n || pa.point_power_w.size() != n)
        return d;

    Long tmax = -1;
    for (Long t : pa.t_offset_s) if (t > tmax) tmax = t;
    if (tmax <= 0) return d;
    const Long mid = tmax / 2;
    const Bool measured = pa.stats.has_measured;

    double p1 = 0, h1 = 0, w1 = 0, p2 = 0, h2 = 0, w2 = 0;
    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > ride::kStopSeconds) continue;
        if (!pts[i].has_hr) continue;
        Real hr = pts[i].hr;
        if (pts[i - 1].has_hr) hr = 0.5 * (pts[i - 1].hr + pts[i].hr);
        const Real pw = measured ? ride::step_power(pts, pa, i, true) : pa.point_power_w[i];
        const Real w  = static_cast<Real>(dt);
        if (pa.t_offset_s[i] <= mid) { p1 += pw * w; h1 += hr * w; w1 += w; }
        else                         { p2 += pw * w; h2 += hr * w; w2 += w; }
    }
    if (w1 <= 0 || w2 <= 0 || h1 <= 0 || h2 <= 0) return d;

    const Real ap1 = p1 / w1, ah1 = h1 / w1, ap2 = p2 / w2, ah2 = h2 / w2;
    if (ah1 <= 0 || ah2 <= 0) return d;
    d.first_ratio  = ap1 / ah1;
    d.second_ratio = ap2 / ah2;
    if (d.first_ratio <= 0) return d;
    d.pct   = (d.first_ratio - d.second_ratio) / d.first_ratio * 100.0;
    d.valid = true;
    return d;
}

} // namespace metrics
