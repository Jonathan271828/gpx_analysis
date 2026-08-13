#pragma once

/// @file palette.hpp
/// @brief The training-zone colours, shared by every chart that uses them.

#include "zones.hpp"

#include "imgui.h"

#include <cstddef>
#include <string>

namespace gui {

/// @brief Colour for zone @p index (0-based). Wraps if the index runs past the
/// palette, so a zone scheme with more entries still draws.
/// @param index Zero-based zone index.
/// @return The zone's colour, packed for ImGui's draw list.
ImU32 zone_colour(std::size_t index);

/// @copydoc zone_colour
ImVec4 zone_colour_vec(std::size_t index);

/// @brief Colour for series @p index of a multi-series chart.
///
/// The same documented categorical slots in the same fixed order as
/// zone_colour(). Kept as a separate name because the meaning differs: here a
/// colour identifies a data series, not a training zone. The two never share a
/// chart, so reusing the hues cannot be ambiguous within one plot.
/// @param index Zero-based series index; wraps past the palette.
/// @return The series colour.
ImVec4 series_colour(std::size_t index);

/// @brief Draw an inline colour swatch for zone @p index and advance the cursor
/// past it, so a label can follow with ImGui::SameLine().
/// @param index Zero-based zone index.
void zone_swatch(std::size_t index);

/// @brief A compact one-line key of swatch + zone name, for charts that colour
/// by zone but do not list the zones themselves.
/// @param table Supplies the labels and bounds; nothing is drawn if invalid.
void draw_zone_legend(const zones::ZoneTable& table);

/// @brief The zone's bounds as text, e.g. "168-229 W" or "458+ W".
///
/// Lives here with the rest of the zone presentation because two charts and
/// the legend all need it, and had each formatted it themselves.
/// @param zone The zone.
/// @param unit Unit to append, e.g. "W" or "bpm".
/// @return The formatted bounds; an open-topped zone reads as "<lo>+ <unit>".
std::string zone_bounds_text(const zones::Zone& zone, const std::string& unit);

/// @brief The zone's full name, e.g. "Z4 Threshold".
/// @param table The distribution the zone belongs to.
/// @param index Zero-based zone index.
/// @return Empty when @p index is negative or outside @p table.
std::string zone_label(const zones::ZoneTable& table, int index);

} // namespace gui
