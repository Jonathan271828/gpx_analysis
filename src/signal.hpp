#pragma once

#include "types.hpp"

#include <cstddef>
#include <vector>

// ---------------------------------------------------------------------------
// Autocorrelation + power spectrum of a time-dependent channel
// ---------------------------------------------------------------------------
//
// A GPX channel (velocity, heart rate, power, cadence) is an irregularly
// sampled time series. compute_acf_psd() resamples it onto a uniform grid,
// removes the mean, and applies the Wiener–Khinchin theorem via an FFT: the
// power spectrum is |FFT(signal)|^2 and the autocorrelation is the inverse
// FFT of that power spectrum. One pair of transforms therefore yields both
// the autocorrelation function and its power spectrum.
//
// The lag/ACF pair lives in the time domain and the freq/PSD pair in the
// frequency domain, so they have different natural lengths. The four vectors
// below are padded to a common length so the result writes as a rectangular
// 4-column table; entries past a column's meaningful range are NaN.

struct SpectralResult {
    Bool                valid     = false;
    Real              dt_s      = 0.0;  // uniform grid spacing actually used (s)
    Size         n_samples = 0;    // length of the resampled series (M)

    std::vector<Real> lag_s;    // column 1: time lag (s); NaN past lag M-1
    std::vector<Real> acf;      // column 2: normalized autocorrelation (acf[0]=1)
    std::vector<Real> freq_hz;  // column 3: frequency (Hz), 0 .. Nyquist
    std::vector<Real> psd;      // column 4: one-sided power spectral density
};

/// Compute the autocorrelation function and its power spectrum for an
/// irregularly sampled time series. `t_s[i]` are sample times in seconds,
/// ascending; `values[i]` the channel values (same length). The series is
/// linearly resampled onto a uniform grid of spacing `dt_s` — pass dt_s <= 0
/// to auto-pick the median sample interval (clamped to at least 1 s).
///
/// Returns valid == false when there is too little data (< 4 samples or a span
/// shorter than a few grid steps) or the signal is constant (zero variance).
SpectralResult compute_acf_psd(const std::vector<Real>& t_s,
                               const std::vector<Real>& values,
                               Real dt_s = 0.0);
