#pragma once

/// @file app_window.hpp
/// @brief The GUI's window contents and the state behind them.

#include "analysis.hpp"
#include "hill_chart.hpp"      // HillAxis
#include "peaks_chart.hpp"     // HoldRef
#include "signal_view.hpp"     // TimeRange
#include "spectral_view.hpp"   // Spectrum

#include <string>
#include <vector>

namespace gui {

/// @brief Everything the window shows, and the loaded file behind it.
///
/// One instance lives for the run and owns both the analysis result and the
/// view state around it. draw() is called once per frame; every panel reads
/// from @ref result_, so nothing is recomputed while idling.
class AppWindow {
public:
    /// @brief Draw one frame of the whole interface.
    void draw();

    /// @brief Load and analyse a GPX file, replacing what is displayed.
    ///
    /// Runs the full command-line analysis and captures its report, then
    /// resets the per-file view state. Safe to call with a path that does not
    /// exist: the error is shown in the banner instead of the report.
    ///
    /// @param path Path to the .gpx file. An empty string is ignored.
    void load(const std::string& path);

private:
    /// @brief The load button, file name, reload and per-file settings.
    void draw_toolbar();

    /// @brief The warning/error strip shown above the tabs, if any.
    void draw_banner();

    /// @brief The report page: the captured text with charts spliced in.
    void draw_report();

    /// @brief The track selector, shown only when the file holds several.
    void draw_track_selector();

    /// @brief Time-in-zone chart panel. @param width Panel width in pixels.
    void draw_zone_panel(float width);

    /// @brief Climb-profile panel, one plot per climb.
    /// @param width Panel width in pixels.
    void draw_hill_panel(float width);

    /// @brief Peak-effort chart and hold curve.
    /// @param width Panel width in pixels.
    void draw_peaks_panel(float width);

    /// @brief The channel picker and Compute button, shared by both spectral
    /// tabs so the selection can be changed from either.
    /// @return The page width available for the plots below.
    float draw_spectral_controls();

    /// @brief One checkbox per channel the current track carries.
    void draw_channel_picker();

    /// @brief The resample interval, the Compute button and the export button.
    void draw_transform_controls();

    /// @brief Path box and button that plot a `.acf.dat` file's columns as they
    /// stand, for comparing a stored spectrum against a recomputed one.
    void draw_dat_loader();

    /// @brief The signals tab: every channel plotted against elapsed time.
    void draw_signals_tab();

    /// @brief The autocorrelation tab.
    void draw_acf_tab();

    /// @brief The power-spectrum tab.
    void draw_psd_tab();

    /// @brief Transform every ticked channel into @ref spectra_.
    ///
    /// Calls signal::compute_acf_psd() on the channels of the current track --
    /// the same call the command line's `--acf-*` flags make -- so the plots
    /// and those files carry the same numbers.
    void compute_spectra();

    /// @brief Write @ref spectra_ next to the loaded GPX, in the same 4-column
    /// format the command line's `--acf-*` flags produce, so the two can be
    /// diffed rather than compared by eye.
    void dump_spectra();

    /// @brief Size the per-channel selection to the current track and default
    /// it to velocity, discarding any spectra computed for the previous file.
    void reset_channel_selection();

    std::string path_;            ///< Full path of the loaded file ("" = none).
    std::string start_dir_;       ///< Where the file dialog opens next.
    Result      result_;          ///< Captured report and chart data for @ref path_.
    int         max_print_ = 10;  ///< Track points to list (mirrors `--points`).
    int         track_     = 0;   ///< Which track the chart views show.
    HillAxis    hill_axis_ = HillAxis::Distance;   ///< X axis of the hill profiles.
    HoldRef     hold_ref_  = HoldRef::Peak;        ///< Reference for the hold curve.

    /// Whether to apply historical wind, mirroring the command line's `--wind`.
    /// Off by default: it is the only control here that reaches the network, so
    /// it stays opt-in. Toggling it re-runs the analysis, because the headwind
    /// term changes estimated power and everything derived from it.
    bool wind_on_ = false;

    std::vector<char>     signal_on_;        ///< Which channels the signals tab plots.
    TimeRange             signal_range_;     ///< Shared x span of the signal plots.
    std::vector<char>     chan_on_;          ///< One flag per channel of @ref track_.
    std::vector<Spectrum> spectra_;          ///< Last computed transforms.
    float                 acf_dt_   = 0.0f;  ///< Resample interval (s); 0 = auto.
    bool                  full_lag_ = false; ///< Show the whole lag range.
    /// Set when @ref spectra_ changes, cleared once the power-spectrum tab has
    /// drawn with it: the signal for those plots to re-fit their axes.
    bool                  psd_refit_ = false;
    char                  dat_path_[1024] = "";  ///< `.acf.dat` to plot verbatim.
    std::string           spectral_note_;   ///< Why a channel produced nothing.
};

} // namespace gui
