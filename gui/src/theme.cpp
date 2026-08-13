#include "theme.hpp"

#include "implot.h"

namespace gui::theme {

const ImVec4 kTextPrimary   = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);  // #ffffff
const ImVec4 kTextSecondary = ImVec4(0.765f, 0.761f, 0.718f, 1.00f);  // #c3c2b7
const ImVec4 kTextMuted     = ImVec4(0.765f, 0.761f, 0.718f, 0.55f);
const ImVec4 kSurface       = ImVec4(0.102f, 0.102f, 0.098f, 1.00f);  // #1a1a19
const ImVec4 kGrid          = ImVec4(1.000f, 1.000f, 1.000f, 0.09f);
const ImVec4 kCurve         = ImVec4(0.224f, 0.529f, 0.898f, 1.00f);  // #3987e5
const ImVec4 kNoZone        = ImVec4(0.600f, 0.600f, 0.580f, 1.00f);
const ImVec4 kWarning       = ImVec4(0.950f, 0.750f, 0.250f, 1.00f);
const ImVec4 kError         = ImVec4(1.000f, 0.450f, 0.400f, 1.00f);

void text_coloured(const ImVec4& colour, const std::string& s) {
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopStyleColor();
}

void value_tooltip(const char* primary, const char* secondary) {
    if (!ImGui::BeginTooltip()) return;
    ImGui::TextUnformatted(primary);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
    ImGui::TextUnformatted(secondary);
    ImGui::PopStyleColor();
    ImGui::EndTooltip();
}

namespace {
// One entry per PushStyleColor in the constructor.
constexpr int kPlotStyleColours = 3;
} // namespace

PlotStyleScope::PlotStyleScope() {
    ImPlot::PushStyleColor(ImPlotCol_FrameBg,  kSurface);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,   kSurface);
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, kGrid);
}

PlotStyleScope::~PlotStyleScope() {
    ImPlot::PopStyleColor(kPlotStyleColours);
}

} // namespace gui::theme
