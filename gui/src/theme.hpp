#pragma once

/// @file theme.hpp
/// @brief The colours every chart shares, and scope guards for the ImGui and
///        ImPlot state that must be pushed and popped in pairs.
///
/// The colours lived as private copies in four files, which is how
/// @c kTextSecondary came to be written with two different alpha literals for
/// the same shade. One definition each, here.

#include "imgui.h"

#include <string>

namespace gui::theme {

// --- palette ---------------------------------------------------------------
// Named for the role they play, not the colour they are, so a change of shade
// does not turn every use site into a lie.

extern const ImVec4 kTextPrimary;    ///< Values the eye should land on first.
extern const ImVec4 kTextSecondary;  ///< Units, captions, supporting figures.
extern const ImVec4 kTextMuted;      ///< Present but deliberately recessive.
extern const ImVec4 kSurface;        ///< Panel and plot background.
extern const ImVec4 kGrid;           ///< Plot grid lines.
extern const ImVec4 kCurve;          ///< The default single-series curve.
extern const ImVec4 kNoZone;         ///< Fill for a stretch with no power.
extern const ImVec4 kWarning;        ///< Warnings that are not failures.
extern const ImVec4 kError;          ///< Failures.

/// @brief Draw one line of text in @p colour.
/// @param colour Text colour.
/// @param s      The text; drawn verbatim, so `%` needs no escaping.
void text_coloured(const ImVec4& colour, const std::string& s);

/// @brief Applies the shared plot background and grid for its lifetime.
///
/// ImPlot's colour stack must be popped exactly as often as it is pushed, and
/// the count was previously repeated as a literal `PopStyleColor(3)` at each
/// call site -- so adding a fourth push meant remembering to find and correct
/// every one of them. Scoping it makes the pairing structural.
class PlotStyleScope {
public:
    PlotStyleScope();
    ~PlotStyleScope();

    PlotStyleScope(const PlotStyleScope&)            = delete;
    PlotStyleScope& operator=(const PlotStyleScope&) = delete;
};

} // namespace gui::theme
