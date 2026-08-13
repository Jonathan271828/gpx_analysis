#pragma once

/// @file panel.hpp
/// @brief The bordered card the report page puts its charts in.

#include "imgui.h"

namespace gui {

/// @brief A bordered, inset child region, closed when the scope ends.
///
/// The three chart panels on the report page each repeated the same eight
/// calls -- push the background, begin the child, pad, indent ... unindent,
/// end, pop, then a trailing gap -- with the padding written as literals every
/// time. Any of those is easy to leave out of one panel and not the others,
/// and ImGui only complains at run time, one frame later.
///
/// Construct one, draw into it, let it fall out of scope:
/// @code
///   {
///       PanelScope panel("zones", ImVec2(width, height));
///       draw_zone_chart(table);
///   }   // closed, and the gap below it added
/// @endcode
class PanelScope {
public:
    /// @param id   ImGui id for the child region; must be unique in its window.
    /// @param size Panel size in pixels.
    PanelScope(const char* id, const ImVec2& size);
    ~PanelScope();

    PanelScope(const PanelScope&)            = delete;
    PanelScope& operator=(const PanelScope&) = delete;

    /// @brief Height the panel itself costs, around whatever it contains.
    ///
    /// Callers size a panel before drawing into it, so this has to be available
    /// separately from the drawing. It covers only the scope's own chrome --
    /// the window padding and the top gap -- because each panel spaces its
    /// contents differently; add that spacing to this.
    /// @return The chrome height in pixels.
    static float chrome_height();
};

} // namespace gui
