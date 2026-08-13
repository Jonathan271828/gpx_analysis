#include "cp_model.hpp"

#include "ride.hpp"

#include <cmath>
#include <vector>

namespace cp {

CpFit fit(const PowerCurve& curve) {
    CpFit f;
    if (!curve.valid) return f;

    // Prefer the measured curve when the ride carried a power meter.
    const Bool measured = curve.has_measured && !curve.meas_power_w.empty();
    const std::vector<Real>& pw = measured ? curve.meas_power_w : curve.est_power_w;
    if (pw.size() != curve.duration_s.size()) return f;

    // Regress P (y) on 1/t (x) over 2–20 min efforts: slope = W', intercept = CP.
    std::vector<double> xs, ys;
    std::vector<Long>   used;
    for (Size i = 0; i < curve.duration_s.size(); ++i) {
        const Long d = curve.duration_s[i];
        if (d >= 120 && d <= 1200) {
            xs.push_back(1.0 / static_cast<double>(d));
            ys.push_back(pw[i]);
            used.push_back(d);
        }
    }
    const int N = static_cast<int>(xs.size());
    if (N < 2) return f;

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < N; ++i) {
        sx += xs[i]; sy += ys[i]; sxx += xs[i] * xs[i]; sxy += xs[i] * ys[i];
    }
    const double den = N * sxx - sx * sx;
    if (std::fabs(den) < 1e-12) return f;
    const double w_prime = (N * sxy - sx * sy) / den;   // slope
    const double cp      = (sy - w_prime * sx) / N;     // intercept
    if (cp <= 0.0 || w_prime <= 0.0) return f;

    f.valid          = true;
    f.measured       = measured;
    f.cp_w           = static_cast<Real>(cp);
    f.w_prime_j      = static_cast<Real>(w_prime);
    f.est_ftp_w      = static_cast<Real>(cp);
    f.n_points       = N;
    f.used_durations = used;
    return f;
}

// ---------------------------------------------------------------------------
// wbal_series — Skiba–Clarke integral W'-balance
// ---------------------------------------------------------------------------

std::vector<WbalSample> wbal_series(const Track& track, const PowerAnalysis& pa,
                                    Real cp_w, Real w_prime_j) {
    std::vector<WbalSample> out;
    if (cp_w <= 0.0 || w_prime_j <= 0.0) return out;

    const Size n = track.points.size();
    if (pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return out;

    Real wbal = w_prime_j;
    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > ride::kStopSeconds) continue;  // gap / stop
        const Real p = pa.point_power_w[i];
        if (p > cp_w)
            wbal -= (p - cp_w) * static_cast<Real>(dt);
        else
            wbal += (cp_w - p) * (w_prime_j - wbal) / w_prime_j * static_cast<Real>(dt);
        if (wbal > w_prime_j) wbal = w_prime_j;           // cap at full reserve
        out.push_back({pa.t_offset_s[i], wbal, p});
    }
    return out;
}

// ---------------------------------------------------------------------------
// count_matches
// ---------------------------------------------------------------------------

MatchStats count_matches(const std::vector<WbalSample>& series, Real w_prime_j) {
    MatchStats m;
    if (series.empty() || w_prime_j <= 0.0) return m;

    const Real low  = 0.50 * w_prime_j;
    const Real high = 0.75 * w_prime_j;

    Real min_j  = series.front().wbal_j;
    Bool armed  = series.front().wbal_j >= high;   // ready to count a new match
    for (const WbalSample& s : series) {
        if (s.wbal_j < min_j) min_j = s.wbal_j;
        if (armed && s.wbal_j < low) { ++m.matches; armed = false; }
        else if (!armed && s.wbal_j > high) armed = true;
    }

    m.valid   = true;
    m.min_j   = min_j;
    m.min_pct = 100.0 * min_j / w_prime_j;
    m.end_j   = series.back().wbal_j;
    m.end_pct = 100.0 * series.back().wbal_j / w_prime_j;
    return m;
}

} // namespace cp
