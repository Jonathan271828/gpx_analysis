#pragma once

/// @file bar_row.hpp
/// @brief The row-of-horizontal-bars chart vocabulary, shared by the zone and
///        peak-effort charts.
///
/// Both draw the same thing with different labels: one row per entry, a
/// recessive full-width track, a coloured fill proportional to the value, and
/// text columns either side. Every metric here -- bar height, corner radius,
/// row padding, hover tint -- was previously declared twice with identical
/// values, as were the row-height and chart-height formulas, so the two charts
/// could drift apart one careless edit at a time.

#include "imgui.h"

#include <cstddef>
#include <string>

namespace gui::bar {

constexpr float kBarHeight  = 14.0f;  ///< Thin marks: the label carries the value.
constexpr float kBarRound   = 4.0f;   ///< Corner radius of track and fill.
constexpr float kRowPad     = 7.0f;   ///< Vertical breathing room per row.
constexpr float kHeadingGap = 6.0f;   ///< Between the heading and the first row.

constexpr ImU32 kTrack    = IM_COL32(255, 255, 255, 18);  ///< Unfilled bar track.
constexpr ImU32 kRowHover = IM_COL32(255, 255, 255, 12);   ///< Whole-row highlight.

/// @brief Height of one row: the taller of text and bar, plus padding.
float row_height();

/// @brief Total height of a bar chart with @p rows rows.
///
/// Heading line, then the gap spacer -- each of which also costs one
/// ItemSpacing.y as ImGui advances past it -- then the rows themselves, which
/// the charts position by hand at exactly row_height() apart.
/// @param rows Number of rows the chart will draw (use 1 for an empty message).
/// @return The height in pixels.
float chart_height(std::size_t rows);

/// @brief Tint the row under the cursor.
/// @param draw    Draw list to render into.
/// @param row_min Top-left of the row.
/// @param row_max Bottom-right of the row.
void highlight_row(ImDrawList* draw, const ImVec2& row_min, const ImVec2& row_max);

/// @brief Draw one bar: the full-width track, then the proportional fill.
///
/// A fill shorter than the corner radius would render as a smear, so it is
/// floored at one full rounded cap -- which is why a zone with a few seconds in
/// it still shows something rather than nothing.
///
/// @param draw     Draw list to render into.
/// @param top_left Top-left corner of the track.
/// @param area     Full track width in pixels.
/// @param fraction Fill as a fraction of @p area; <= 0 draws the track alone.
/// @param colour   Fill colour.
void draw_bar(ImDrawList* draw, const ImVec2& top_left, float area,
              float fraction, ImU32 colour);

/// @brief Draw text at an exact screen position, in @p colour.
///
/// The rows are laid out by hand rather than by ImGui's cursor, so each text
/// column is placed rather than flowed.
/// @param pos    Top-left screen position for the text.
/// @param colour Text colour.
/// @param s      The text; drawn verbatim.
void text_at(const ImVec2& pos, const ImVec4& colour, const std::string& s);

} // namespace gui::bar
