#pragma once

/// @file profile_chart.hpp
/// @brief The rider's power-duration shape, drawn as a departure from balance.

#include "profile.hpp"   // profile::Profile

namespace gui {

/// @brief Draw the profile: one row per duration, deviation from balance.
///
/// Diverging bars from a centre line rather than a bar chart from zero, because
/// the quantity is signed and its sign is the whole message: right of the line
/// is a strength, left is a weakness. A chart growing from zero would put the
/// strongest and weakest durations at opposite ends of a long scale and hide
/// the one thing worth seeing, which is the tilt of the curve.
///
/// @param p     The profile; nothing is drawn when it is invalid.
/// @param width Panel width in pixels.
void draw_profile_chart(const profile::Profile& p, float width);

/// @brief Height in pixels that draw_profile_chart() needs for @p p.
///
/// Reads the live font metrics, so it must be called inside a frame and in the
/// same style as the draw; the two cannot then drift.
/// @param p The profile it would draw.
/// @return The height, verdict and caveat lines included.
float profile_chart_height(const profile::Profile& p);

} // namespace gui
