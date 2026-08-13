#include "bar_row.hpp"

#include "theme.hpp"

#include <algorithm>

namespace gui::bar {

namespace {
constexpr float kRowHoverRound = 3.0f;
} // namespace

float row_height() {
    return std::max(ImGui::GetTextLineHeight(), kBarHeight) + kRowPad;
}

float chart_height(std::size_t rows) {
    return ImGui::GetTextLineHeight() + kHeadingGap +
           ImGui::GetStyle().ItemSpacing.y * 2.0f +
           row_height() * static_cast<float>(rows);
}

void highlight_row(ImDrawList* draw, const ImVec2& row_min, const ImVec2& row_max) {
    draw->AddRectFilled(row_min, row_max, kRowHover, kRowHoverRound);
}

void draw_bar(ImDrawList* draw, const ImVec2& top_left, float area,
              float fraction, ImU32 colour) {
    const ImVec2 track_end(top_left.x + area, top_left.y + kBarHeight);
    draw->AddRectFilled(top_left, track_end, kTrack, kBarRound);

    if (fraction <= 0.0f) return;

    const float length = std::max(fraction * area, kBarRound * 2.0f);
    draw->AddRectFilled(top_left,
                        ImVec2(top_left.x + length, top_left.y + kBarHeight),
                        colour, kBarRound);
}

void text_at(const ImVec2& pos, const ImVec4& colour, const std::string& s) {
    ImGui::SetCursorScreenPos(pos);
    theme::text_coloured(colour, s);
}

} // namespace gui::bar
