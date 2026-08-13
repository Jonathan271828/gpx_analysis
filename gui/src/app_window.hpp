#pragma once

/// @file app_window.hpp
/// @brief The GUI's window contents and the state behind them.

#include "analysis.hpp"
#include "hill_chart.hpp"      // HillAxis
#include "peaks_chart.hpp"     // HoldRef
#include "spectral_view.hpp"   // Spectrum

#include <string>
#include <vector>

namespace gui {

/// @brief Everything the window shows, and the loaded file behind it.
class AppWindow {
public:
    /// @brief Draw one frame of the whole interface.
    void draw();

    /// @brief Load and analyse a GPX file, replacing what is displayed.
    /// Safe to call with a path that does not exist: the error is shown in the
    /// banner instead of the report.
    void load(const std::string& path);

private:
    void draw_toolbar();
    void draw_wind_controls();
    void draw_banner();
    void draw_report();
    void draw_zone_panel(float width);
    void draw_hill_panel(float width);
    void draw_peaks_panel(float width);

    // Shared by both spectral tabs: which channels to transform, and the button
    // that does it. Returns the usable page width.
    float draw_spectral_controls();
    void  draw_dat_loader();
    void  draw_acf_tab();
    void  draw_psd_tab();
    void  compute_spectra();

    // Write the computed spectra next to the loaded GPX, in the same 4-column
    // format the CLI's --acf-* flags produce, so the two can be diffed.
    void  dump_spectra();

    // Per-channel selection sized to the current track's channel list.
    void reset_channel_selection();

    std::string path_;            // full path of the loaded file ("" = none)
    std::string start_dir_;       // where the file dialog opens next
    Result      result_;          // captured report for path_
    int         max_print_ = 10;  // track points to list (matches the CLI)
    int         track_     = 0;   // which track the chart views show
    HillAxis    hill_axis_ = HillAxis::Distance;   // x axis of the hill profiles
    HoldRef     hold_ref_  = HoldRef::Peak;        // reference for the hold curve

    // Wind source for the power model. Cached by default: without the headwind
    // term the estimated power -- and its spectrum -- does not match what the
    // command line produces, and every reference spectrum in test/ was made with
    // wind. The first load of a ride fetches once and writes
    // "<gpx>.wind.json"; later loads read that back without going online.
    WindSource  wind_src_ = WindSource::Cache;
    std::string wind_path_;

    std::vector<char>     chan_on_;         // one flag per channel of track_
    std::vector<Spectrum> spectra_;         // last computed transforms
    float                 acf_dt_   = 0.0f; // resample interval (s); 0 = auto
    bool                  full_lag_ = false;// show the whole lag range
    // Set when spectra_ changes, cleared once the PSD tab has drawn with it: the
    // signal for those plots to re-fit their axes to the new data.
    bool                  psd_refit_ = false;
    char                  dat_path_[1024] = "";  // .acf.dat to plot verbatim
    std::string           spectral_note_;   // why a channel produced nothing
};

} // namespace gui
