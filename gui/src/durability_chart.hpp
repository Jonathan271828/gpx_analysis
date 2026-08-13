#pragma once

/// @file durability_chart.hpp
/// @brief The power-duration curve's decay as work accumulates.

#include "durability.hpp"   // durability::Report

namespace gui {

/// @brief Draw one line per duration: best power against work already done.
///
/// Work is on the x axis rather than time, because that is what the table
/// itself is indexed by and what makes rides of different length comparable --
/// two hours of climbing and four hours of flat are not the same fatigue. A
/// line that sags to the right is a rider losing power as the ride goes on;
/// one that stays flat is the durability the table is measuring.
///
/// @param report The report; nothing is drawn when it is invalid.
/// @param width  Plot width in pixels.
void draw_durability_chart(const durability::Report& report, float width);

/// @brief Height in pixels that draw_durability_chart() needs.
/// @return The plot height, heading and axis labels included.
float durability_chart_height();

} // namespace gui
