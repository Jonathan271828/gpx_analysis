#include "signal.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace {

/// In-place iterative radix-2 Cooley–Tukey FFT. `sign = -1` is the forward
/// transform, `sign = +1` the inverse (unscaled — the caller divides by N).
/// `a.size()` must be a power of two.
void fft(std::vector<std::complex<double>>& a, int sign) {
    const std::size_t n = a.size();
    if (n < 2) return;

    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Butterfly stages.
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2.0 * M_PI / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k]             = u + v;
                a[i + k + len / 2]   = u - v;
                w *= wlen;
            }
        }
    }
}

/// Smallest power of two that is >= x (at least 1).
std::size_t next_pow2(std::size_t x) {
    std::size_t n = 1;
    while (n < x) n <<= 1;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// compute_acf_psd
// ---------------------------------------------------------------------------

SpectralResult compute_acf_psd(const std::vector<double>& t_s,
                               const std::vector<double>& values,
                               double dt_s)
{
    SpectralResult R;
    const std::size_t k = t_s.size();
    if (k < 4 || values.size() != k) return R;

    // --- Grid spacing: auto = median positive sample interval (>= 1 s) -------
    if (dt_s <= 0.0) {
        std::vector<double> deltas;
        deltas.reserve(k - 1);
        for (std::size_t i = 1; i < k; ++i) {
            const double d = t_s[i] - t_s[i - 1];
            if (d > 0.0) deltas.push_back(d);
        }
        if (deltas.empty()) return R;
        std::nth_element(deltas.begin(), deltas.begin() + deltas.size() / 2,
                         deltas.end());
        dt_s = deltas[deltas.size() / 2];
        if (dt_s < 1.0) dt_s = 1.0;
    }
    R.dt_s = dt_s;

    // --- Resample onto a uniform grid by linear interpolation ---------------
    const double t0   = t_s.front();
    const double span = t_s.back() - t0;
    if (span < 3.0 * dt_s) return R;                 // too short to be useful
    const std::size_t M = static_cast<std::size_t>(span / dt_s) + 1;
    if (M < 4) return R;

    std::vector<double> y(M, 0.0);
    std::size_t seg = 0;                             // bracketing sample index
    for (std::size_t j = 0; j < M; ++j) {
        const double t = t0 + static_cast<double>(j) * dt_s;
        while (seg + 1 < k && t_s[seg + 1] < t) ++seg;
        if (seg + 1 >= k) { y[j] = values.back(); continue; }  // clamp at end
        const double ta = t_s[seg],   tb = t_s[seg + 1];
        const double va = values[seg], vb = values[seg + 1];
        const double denom = tb - ta;
        double f = (denom > 0.0) ? (t - ta) / denom : 0.0;
        if (f < 0.0) f = 0.0; else if (f > 1.0) f = 1.0;
        y[j] = va + f * (vb - va);
    }

    // --- Mean-subtract (autocovariance / fluctuation spectrum) --------------
    double mean = 0.0;
    for (double v : y) mean += v;
    mean /= static_cast<double>(M);
    for (double& v : y) v -= mean;

    // --- Forward FFT of the zero-padded signal ------------------------------
    // Pad to N >= 2M so the inverse transform of the power spectrum yields the
    // *linear* (non-circular) autocorrelation for lags 0 .. M-1.
    const std::size_t N = next_pow2(2 * M);
    std::vector<std::complex<double>> X(N, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < M; ++i) X[i] = y[i];
    fft(X, -1);

    // --- Power spectrum |X|^2 -----------------------------------------------
    std::vector<std::complex<double>> S(N);
    for (std::size_t j = 0; j < N; ++j) {
        const double re = X[j].real(), im = X[j].imag();
        S[j] = std::complex<double>(re * re + im * im, 0.0);
    }

    // One-sided PSD estimate: (dt / M) * |X|^2, with interior bins doubled to
    // fold in the negative frequencies. Units: (value^2) / Hz.
    const std::size_t half     = N / 2;
    const double      psd_norm = dt_s / static_cast<double>(M);
    R.freq_hz.resize(half + 1);
    R.psd.resize(half + 1);
    for (std::size_t j = 0; j <= half; ++j) {
        R.freq_hz[j] = static_cast<double>(j) / (static_cast<double>(N) * dt_s);
        double p = S[j].real() * psd_norm;
        if (j != 0 && j != half) p *= 2.0;
        R.psd[j] = p;
    }

    // --- Autocorrelation via inverse FFT of the power spectrum --------------
    fft(S, +1);                                      // S now holds autocovariance*N
    const double c0 = S[0].real();                   // ~ N * M * variance
    if (!(c0 > 0.0)) return R;                        // constant / degenerate

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    R.lag_s.resize(half + 1);
    R.acf.resize(half + 1);
    for (std::size_t lag = 0; lag <= half; ++lag) {
        if (lag < M) {                               // meaningful lag range
            R.lag_s[lag] = static_cast<double>(lag) * dt_s;
            R.acf[lag]   = S[lag].real() / c0;       // normalized: acf[0] = 1
        } else {                                     // pad to match freq length
            R.lag_s[lag] = NaN;
            R.acf[lag]   = NaN;
        }
    }

    R.n_samples = M;
    R.valid     = true;
    return R;
}
