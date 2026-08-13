#pragma once

/// @file spectral_view.hpp
/// @brief Autocorrelation and power-spectrum plots for the ride's channels.

#include "signal.hpp"   // signal::SpectralResult

#include <string>
#include <vector>

namespace gui {

/// @brief One channel's computed autocorrelation and power spectrum.
struct Spectrum {
    std::string            name;     ///< Channel name, e.g. "velocity".
    std::string            unit;     ///< Channel unit, e.g. "km/h".
    signal::SpectralResult result;   ///< The transform output.
};

/// @brief Read a spectrum back from a 4-column file written by
/// io::write_spectral_file (the CLI's --acf-* output).
///
/// The columns are taken verbatim -- `lag_s | acf | freq_hz | psd` -- so what is
/// plotted is exactly what is in the file, with no transform applied. Use this
/// to display a spectrum the command line produced instead of recomputing it,
/// which is the only way to see one that was generated under options the GUI
/// cannot reproduce (a wind term, a non-default mass or CdA).
///
/// Columns 1-2 are the literal text `nan` past lag M-1. Those rows are kept:
/// dropping them would silently truncate the frequency axis, since the PSD runs
/// to Nyquist while the ACF stops at M-1.
///
/// @param path Path to the .dat file.
/// @param out  Out: the loaded spectrum; `result.valid` is false on failure.
/// @param err  Out: reason for failure, suitable for showing to the user.
/// @return True when at least four rows were read.
bool load_spectrum_file(const std::string& path, Spectrum& out, std::string& err);

/// @brief Draw every channel's autocorrelation in one plot.
///
/// The autocorrelations are dimensionless and all start at 1, so they belong on
/// shared axes where their shapes can be compared directly.
///
/// @param spectra  The computed channels; nothing is drawn if empty.
/// @param full_lag When false, the view opens on the short lags where the
///                 structure lives instead of the whole ride's span.
/// @param width    Plot width in pixels.
/// @param height   Plot height in pixels.
void draw_acf_plot(const std::vector<Spectrum>& spectra, bool full_lag,
                   float width, float height);

/// @brief Draw one power spectrum per channel, stacked.
///
/// Unlike the autocorrelations these cannot share a plot: each is in the
/// channel's own units squared per hertz, so one pair of axes cannot carry
/// watts and heart rate at once.
///
/// Each channel keeps its own plot, keyed by name, so its axis range survives a
/// change of selection instead of being inherited from whichever channel
/// happened to occupy the same slot before.
///
/// @param spectra The computed channels; nothing is drawn if empty.
/// @param refit   Pass true on the first frame after a recompute. ImPlot fits a
///                plot's axes only when it first sees the id, so without this a
///                new spectrum is drawn against the previous one's range.
/// @param width   Plot width in pixels.
/// @param height  Height of each individual plot in pixels.
void draw_psd_plots(const std::vector<Spectrum>& spectra, bool refit,
                    float width, float height);

} // namespace gui
