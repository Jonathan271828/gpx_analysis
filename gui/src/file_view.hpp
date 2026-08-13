#pragma once

/// @file file_view.hpp
/// @brief One loaded GPX file: its analysis, and every view onto it.

#include "analysis.hpp"
#include "hill_chart.hpp"      // HillAxis
#include "peaks_chart.hpp"     // HoldRef
#include "signal_view.hpp"     // TimeRange
#include "spectral_view.hpp"   // Spectrum

#include <string>
#include <vector>

namespace gui {

/// @brief Everything belonging to one ride: the captured analysis and the state
/// of the four views onto it.
///
/// Several files are open at once, each with its own tab, so none of this can
/// live on the window: two rides showing different tracks, different channel
/// selections and different zoom levels is the point of having them side by
/// side. Analysis options that describe the *rider* rather than the ride --
/// how many points to list, whether to fetch wind -- stay on the window and are
/// passed in, so all files are analysed alike.
class FileView {
public:
    /// @brief Analyse @p path and prepare its views.
    /// @param path      Path to the .gpx file.
    /// @param max_print Track points to list first (mirrors `--points`).
    /// @param use_wind  Whether to fetch and apply wind (mirrors `--wind`).
    FileView(std::string path, std::size_t max_print, bool use_wind);

    /// @brief Re-run the analysis, keeping the file but discarding view state.
    /// @param max_print Track points to list first.
    /// @param use_wind  Whether to fetch and apply wind.
    void reload(std::size_t max_print, bool use_wind);

    /// @brief Draw the banner and the four view tabs for this file.
    ///
    /// The caller is responsible for pushing an ImGui id unique to this file:
    /// the tab bars, child regions and plots inside all use fixed names, and
    /// ImPlot stores a plot's axis range under its id, so without that two
    /// files would share one zoom.
    void draw();

    /// @brief The file's full path.
    const std::string& path() const { return path_; }

    /// @brief The file name alone, for the tab label.
    std::string title() const;

private:
    void draw_banner();
    void draw_report();
    void draw_track_selector();
    void draw_zone_panel(float width);
    void draw_hill_panel(float width);
    void draw_peaks_panel(float width);

    float draw_spectral_controls();
    void  draw_channel_picker();
    void  draw_transform_controls();
    void  draw_dat_loader();

    void draw_signals_tab();
    void draw_acf_tab();
    void draw_psd_tab();

    void compute_spectra();
    void dump_spectra();
    void reset_channel_selection();

    std::string path_;            ///< Full path of the file.
    Result      result_;          ///< Captured report and chart data.
    int         track_     = 0;   ///< Which track the chart views show.
    HillAxis    hill_axis_ = HillAxis::Distance;   ///< X axis of the hill profiles.
    HoldRef     hold_ref_  = HoldRef::Peak;        ///< Reference for the hold curve.

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
