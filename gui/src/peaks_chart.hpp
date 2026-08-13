#pragma once

/// @file peaks_chart.hpp
/// @brief Bar chart of the mean-maximal power curve (peak power efforts).

#include "analysis.hpp"   // PeakBar
#include "zones.hpp"

#include <vector>

namespace gui {

/// @brief Draw the peak efforts as one horizontal bar per duration.
///
/// Rows run from the shortest window to the longest, so the bars fall away as a
/// staircase -- the shape of the power-duration curve. Each bar is scaled from
/// zero against the best effort of the ride and coloured by the power zone it
/// lands in, with the zone named beside it so colour is never the only cue.
///
/// @param peaks The efforts, ascending by duration. Nothing is drawn if empty.
/// @param zones Zone table, for the zone names.
void draw_peaks_chart(const std::vector<PeakBar>& peaks,
                      const zones::ZoneTable& zones);

/// @brief Height in pixels that draw_peaks_chart() needs for @p peaks.
/// @param peaks The efforts the chart would draw.
/// @return The height in pixels; 0 when @p peaks is empty.
float peaks_chart_height(const std::vector<PeakBar>& peaks);

/// @brief What the hold curve measures power against.
enum class HoldRef {
    Peak,   ///< Percentage of the ride's own best effort.
    Ftp     ///< Percentage of FTP.
};

/// @brief Draw the inverse view: how long each fraction of your power was held.
///
/// The same efforts as draw_peaks_chart(), read the other way round -- power as
/// a percentage on the y axis against the duration it was sustained for on the
/// x axis. Duration is logarithmic because these windows span 5 s to an hour;
/// on a linear scale, or as bars, everything below the longest is invisible.
///
/// @param peaks The efforts, ascending by duration. Nothing drawn if empty.
/// @param ftp_w FTP in watts, for HoldRef::Ftp. Ignored when <= 0.
/// @param ref   Which reference the percentage is taken against.
/// @param width Plot width in pixels.
void draw_hold_curve(const std::vector<PeakBar>& peaks, Real ftp_w,
                     HoldRef ref, float width);

/// @brief Height in pixels that draw_hold_curve() needs.
/// @return The plot height, axis labels included.
float hold_curve_height();

} // namespace gui
