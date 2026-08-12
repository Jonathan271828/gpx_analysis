#include "cp_model.hpp"

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

} // namespace cp
