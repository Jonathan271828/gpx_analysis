#pragma once

/**
 * @file signal.hpp
 * @brief Autocorrelation and power spectrum of a time-dependent channel.
 *
 * A GPX channel (velocity, heart rate, power, cadence) is an irregularly
 * sampled time series. compute_acf_psd() resamples it onto a uniform grid,
 * removes the mean, and applies the Wiener–Khinchin theorem via an FFT: the
 * power spectrum is |FFT(signal)|^2 and the autocorrelation is the inverse FFT
 * of that spectrum. One pair of transforms yields both.
 */

#include "types.hpp"

#include <cstddef>
#include <vector>

namespace signal {

/**
 * @brief Autocorrelation function and power spectrum of one channel.
 *
 * The lag/ACF pair (time domain) and freq/PSD pair (frequency domain) have
 * different natural lengths, so all four vectors are padded to a common length
 * and write as a rectangular 4-column table; entries past a column's
 * meaningful range are NaN.
 */
struct SpectralResult {
    Bool valid     = false;    /**< True if the transform succeeded. */
    Real dt_s      = 0.0;      /**< Uniform grid spacing actually used (s). */
    Size n_samples = 0;        /**< Length of the resampled series (M). */

    std::vector<Real> lag_s;   /**< Column 1: time lag (s); NaN past lag M-1. */
    std::vector<Real> acf;     /**< Column 2: normalized autocorrelation (acf[0]=1). */
    std::vector<Real> freq_hz; /**< Column 3: frequency (Hz), 0 .. Nyquist. */
    std::vector<Real> psd;     /**< Column 4: one-sided power spectral density. */
};

/**
 * @brief Compute the autocorrelation and power spectrum of an irregular series.
 * @param t_s    Sample times in seconds, ascending.
 * @param values Channel values (same length as @p t_s).
 * @param dt_s   Uniform resample spacing; pass <= 0 to auto-pick the median
 *               sample interval (clamped to at least 1 s).
 * @return Filled SpectralResult; valid == false when there is too little data
 *         (< 4 samples or a very short span) or the signal is constant.
 */
SpectralResult compute_acf_psd(const std::vector<Real>& t_s,
                               const std::vector<Real>& values,
                               Real dt_s = 0.0);

} // namespace signal
