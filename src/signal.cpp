#include "signal.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace {

/// In-place iterative radix-2 Cooley–Tukey FFT. `sign = -1` is the forward
/// transform, `sign = +1` the inverse (unscaled — the caller divides by N).
/// `a.size()` must be a power of two.
void fft(std::vector<std::complex<Real>>& a, Int sign) {
    const Size n = a.size();
    if (n < 2) return;

    // Bit-reversal permutation.
    for (Size i = 1, j = 0; i < n; ++i) {
        Size bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Butterfly stages.
    for (Size len = 2; len <= n; len <<= 1) {
        const Real ang = sign * 2.0 * M_PI / static_cast<Real>(len);
        const std::complex<Real> wlen(std::cos(ang), std::sin(ang));
        for (Size i = 0; i < n; i += len) {
            std::complex<Real> w(1.0, 0.0);
            for (Size k = 0; k < len / 2; ++k) {
                const std::complex<Real> u = a[i + k];
                const std::complex<Real> v = a[i + k + len / 2] * w;
                a[i + k]             = u + v;
                a[i + k + len / 2]   = u - v;
                w *= wlen;
            }
        }
    }
}

/// Smallest power of two that is >= x (at least 1).
Size next_pow2(Size x) {
    Size n = 1;
    while (n < x) n <<= 1;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// compute_acf_psd
// ---------------------------------------------------------------------------

namespace signal {

SpectralResult compute_acf_psd(const std::vector<Real>& t_s,
                               const std::vector<Real>& values,
                               Real dt_s)
{
    SpectralResult R;
    const Size k = t_s.size();
    if (k < 4 || values.size() != k) return R;

    // --- Grid spacing: auto = median positive sample interval (>= 1 s) -------
    if (dt_s <= 0.0) {
        std::vector<Real> deltas;
        deltas.reserve(k - 1);
        for (Size i = 1; i < k; ++i) {
            const Real d = t_s[i] - t_s[i - 1];
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
    const Real t0   = t_s.front();
    const Real span = t_s.back() - t0;
    if (span < 3.0 * dt_s) return R;                 // too short to be useful
    const Size M = static_cast<Size>(span / dt_s) + 1;
    if (M < 4) return R;

    std::vector<Real> y(M, 0.0);
    Size seg = 0;                             // bracketing sample index
    for (Size j = 0; j < M; ++j) {
        const Real t = t0 + static_cast<Real>(j) * dt_s;
        while (seg + 1 < k && t_s[seg + 1] < t) ++seg;
        if (seg + 1 >= k) { y[j] = values.back(); continue; }  // clamp at end
        const Real ta = t_s[seg],   tb = t_s[seg + 1];
        const Real va = values[seg], vb = values[seg + 1];
        const Real denom = tb - ta;
        Real f = (denom > 0.0) ? (t - ta) / denom : 0.0;
        if (f < 0.0) f = 0.0; else if (f > 1.0) f = 1.0;
        y[j] = va + f * (vb - va);
    }

    // --- Mean-subtract (autocovariance / fluctuation spectrum) --------------
    Real mean = 0.0;
    for (Real v : y) mean += v;
    mean /= static_cast<Real>(M);
    for (Real& v : y) v -= mean;

    // --- Forward FFT of the zero-padded signal ------------------------------
    // Pad to N >= 2M so the inverse transform of the power spectrum yields the
    // *linear* (non-circular) autocorrelation for lags 0 .. M-1.
    const Size N = next_pow2(2 * M);
    std::vector<std::complex<Real>> X(N, std::complex<Real>(0.0, 0.0));
    for (Size i = 0; i < M; ++i) X[i] = y[i];
    fft(X, -1);

    // --- Power spectrum |X|^2 -----------------------------------------------
    std::vector<std::complex<Real>> S(N);
    for (Size j = 0; j < N; ++j) {
        const Real re = X[j].real(), im = X[j].imag();
        S[j] = std::complex<Real>(re * re + im * im, 0.0);
    }

    // One-sided PSD estimate: (dt / M) * |X|^2, with interior bins doubled to
    // fold in the negative frequencies. Units: (value^2) / Hz.
    const Size half     = N / 2;
    const Real      psd_norm = dt_s / static_cast<Real>(M);
    R.freq_hz.resize(half + 1);
    R.psd.resize(half + 1);
    for (Size j = 0; j <= half; ++j) {
        R.freq_hz[j] = static_cast<Real>(j) / (static_cast<Real>(N) * dt_s);
        Real p = S[j].real() * psd_norm;
        if (j != 0 && j != half) p *= 2.0;
        R.psd[j] = p;
    }

    // --- Autocorrelation via inverse FFT of the power spectrum --------------
    fft(S, +1);                                      // S now holds autocovariance*N
    const Real c0 = S[0].real();                   // ~ N * M * variance
    if (!(c0 > 0.0)) return R;                        // constant / degenerate

    const Real NaN = std::numeric_limits<Real>::quiet_NaN();
    R.lag_s.resize(half + 1);
    R.acf.resize(half + 1);
    for (Size lag = 0; lag <= half; ++lag) {
        if (lag < M) {                               // meaningful lag range
            R.lag_s[lag] = static_cast<Real>(lag) * dt_s;
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

} // namespace signal
