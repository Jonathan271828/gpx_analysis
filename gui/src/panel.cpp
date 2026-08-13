#include "panel.hpp"

#include "theme.hpp"

namespace gui {

namespace {

constexpr float kTopPad    = 4.0f;   ///< Gap above the content.
constexpr float kIndent    = 10.0f;  ///< Inset either side of the content.
constexpr float kGapBelow  = 8.0f;   ///< Separation from whatever follows.

} // namespace

PanelScope::PanelScope(const char* id, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::kSurface);
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    ImGui::Dummy(ImVec2(0.0f, kTopPad));
    ImGui::Indent(kIndent);
}

PanelScope::~PanelScope() {
    ImGui::Unindent(kIndent);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, kGapBelow));
}

float PanelScope::chrome_height() {
    return ImGui::GetStyle().WindowPadding.y * 2.0f + kTopPad;
}

} // namespace gui
