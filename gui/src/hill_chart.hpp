#pragma once

/// @file hill_chart.hpp
/// @brief Elevation profile of a single climb.

#include "analysis.hpp"   // HillProfile

namespace gui {

/// @brief What the elevation profile is plotted against.
enum class HillAxis {
    Distance,   ///< Kilometres from the foot of the climb.
    Time        ///< Minutes from the foot of the climb.
};

/// @brief Draw one climb as a filled elevation profile with a heading.
/// @param hill  The climb to draw.
/// @param index Zero-based position, shown as "Hill N" to match the table.
/// @param axis  Which quantity to put on the x axis. Falls back to distance
///              when the climb has no usable timestamps.
/// @param zones Zone table, used to name the zone under the cursor.
/// @param width Panel width in pixels.
void draw_hill_chart(const HillProfile& hill, int index, HillAxis axis,
                     const zones::ZoneTable& zones, float width);

/// @brief Height in pixels that draw_hill_chart() needs.
/// @return The height of one climb plot, heading included.
float hill_chart_height();

} // namespace gui
