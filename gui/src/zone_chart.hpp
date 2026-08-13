#pragma once

/// @file zone_chart.hpp
/// @brief Horizontal bar chart of time spent in each training zone.

#include "zones.hpp"

namespace gui {

/// @brief Draw a zone table as a labelled horizontal bar chart.
///
/// One row per zone, in zone order: the label, its bound in watts or bpm, a bar
/// whose length is the time spent there, and that time with its share of the
/// total. Bars are scaled to the largest zone so the short ones stay visible;
/// the printed percentages carry the true proportions.
///
/// @param table The distribution to draw. Nothing is drawn when it is invalid.
void draw_zone_chart(const zones::ZoneTable& table);

/// @brief Height in pixels that draw_zone_chart() needs for @p table.
/// Call inside the same frame and style as the draw, so a caller sizing a panel
/// around the chart cannot drift from the chart's own layout.
float zone_chart_height(const zones::ZoneTable& table);

} // namespace gui
