#include "zone_chart.hpp"

#include "io_base.hpp"   // io::format_duration
#include "palette.hpp"   // zone_colour

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>

namespace gui {

namespace {

// Ink and surface tokens. Values never wear a series colour.
const ImVec4 kTextPrimary   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);   // #ffffff
const ImVec4 kTextSecondary = ImVec4(0.765f, 0.761f, 0.718f, 1.0f); // #c3c2b7
const ImVec4 kTextMuted     = ImVec4(0.765f, 0.761f, 0.718f, 0.55f);

constexpr ImU32 kTrackColour = IM_COL32(255, 255, 255, 18);  // recessive bar track
constexpr ImU32 kRowHover    = IM_COL32(255, 255, 255, 12);

constexpr float kBarHeight   = 14.0f;   // thin marks
constexpr float kBarRound    = 4.0f;    // 4px rounded data-end
constexpr float kRowPad      = 7.0f;    // vertical breathing room per row
constexpr float kHeadingGap  = 6.0f;    // between the heading and the first row

float row_height() {
    return std::max(ImGui::GetTextLineHeight(), kBarHeight) + kRowPad;
}

// "168-229 W" / "458+ W" for the zone's bounds.
std::string bounds_text(const zones::Zone& z, const std::string& unit) {
    char buf[64];
    if (z.hi < 0.0) std::snprintf(buf, sizeof buf, "%.0f+ %s", z.lo, unit.c_str());
    else            std::snprintf(buf, sizeof buf, "%.0f-%.0f %s", z.lo, z.hi, unit.c_str());
    return buf;
}

void text_coloured(const ImVec4& colour, const std::string& s) {
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopStyleColor();
}

} // namespace

float zone_chart_height(const zones::ZoneTable& table) {
    const std::size_t rows =
        (table.valid && !table.zones.empty()) ? table.zones.size() : 1;
    // Heading line, then the gap spacer -- each of which also costs one
    // ItemSpacing.y as ImGui advances past it -- then the rows themselves, which
    // draw_zone_chart() positions by hand at exactly row_height() apart.
    return ImGui::GetTextLineHeight() + kHeadingGap +
           ImGui::GetStyle().ItemSpacing.y * 2.0f +
           row_height() * static_cast<float>(rows);
}

void draw_zone_chart(const zones::ZoneTable& table) {
    if (!table.valid || table.zones.empty()) {
        text_coloured(kTextMuted,
                      "No zone distribution for this track (needs power data "
                      "and an FTP setting).");
        return;
    }

    const std::string unit = (table.kind == "power") ? "W" : "bpm";

    // --- Heading: what is being measured, against what, over how long --------
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::Text("Time in %s zones", table.kind.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    text_coloured(kTextSecondary,
                  "  " + table.basis + "  -  total " +
                      io::format_duration(static_cast<Long>(table.total_s)));
    ImGui::Dummy(ImVec2(0.0f, kHeadingGap));

    // --- Column geometry ----------------------------------------------------
    const float swatch_w = ImGui::GetTextLineHeight() * 0.42f;
    float       label_w  = 0.0f;
    float       bounds_w = 0.0f;
    for (const zones::Zone& z : table.zones) {
        label_w  = std::max(label_w, ImGui::CalcTextSize(z.label.c_str()).x);
        bounds_w = std::max(bounds_w,
                            ImGui::CalcTextSize(bounds_text(z, unit).c_str()).x);
    }
    const float value_w  = ImGui::CalcTextSize("00h 00m 00s   100.0 %").x;
    const float gap      = ImGui::GetStyle().ItemSpacing.x;
    const float avail    = ImGui::GetContentRegionAvail().x;
    const float bar_area = std::max(60.0f, avail - swatch_w - label_w - bounds_w -
                                              value_w - 4.0f * gap);

    // Bar length is the zone's share of the total, so the track behind it reads
    // as 100 % and each bar agrees with the percentage printed beside it.
    const Real total = table.total_s > 0.0 ? table.total_s : 1.0;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float row_h = row_height();

    // Rows are placed by hand at exactly row_h apart. Letting ImGui advance the
    // cursor per row would add an ItemSpacing.y each time, which the height
    // zone_chart_height() reports would not account for.
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    for (Size i = 0; i < table.zones.size(); ++i) {
        const zones::Zone& z   = table.zones[i];
        const Real         pct = table.total_s > 0.0 ? 100.0 * z.seconds / table.total_s : 0.0;

        const ImVec2 row_min(origin.x, origin.y + row_h * static_cast<float>(i));
        const ImVec2 row_max = ImVec2(row_min.x + avail, row_min.y + row_h);

        // Whole-row hover: highlight, plus the exact numbers in a tooltip.
        const bool hovered = ImGui::IsWindowHovered() &&
                             ImGui::IsMouseHoveringRect(row_min, row_max);
        if (hovered) {
            draw->AddRectFilled(row_min, row_max, kRowHover, 3.0f);
            ImGui::SetTooltip("%s\n%s\n%s  (%.1f %% of %s)",
                              z.label.c_str(), bounds_text(z, unit).c_str(),
                              io::format_duration(static_cast<Long>(z.seconds)).c_str(),
                              pct, io::format_duration(static_cast<Long>(table.total_s)).c_str());
        }

        // A swatch keeps the colour-to-zone mapping legible even for a zone with
        // no time in it, which has no bar to carry its colour.
        const float text_dy = (row_h - ImGui::GetTextLineHeight()) * 0.5f;
        const float sw_y    = row_min.y + (row_h - swatch_w) * 0.5f;
        draw->AddRectFilled(ImVec2(row_min.x, sw_y),
                            ImVec2(row_min.x + swatch_w, sw_y + swatch_w),
                            zone_colour(i), 2.0f);

        ImGui::SetCursorScreenPos(
            ImVec2(row_min.x + swatch_w + gap, row_min.y + text_dy));
        text_coloured(kTextPrimary, z.label);

        ImGui::SetCursorScreenPos(
            ImVec2(row_min.x + swatch_w + label_w + 2.0f * gap, row_min.y + text_dy));
        text_coloured(kTextSecondary, bounds_text(z, unit));

        // The bar.
        const float bar_x0 = row_min.x + swatch_w + label_w + bounds_w + 3.0f * gap;
        const float bar_y0 = row_min.y + (row_h - kBarHeight) * 0.5f;
        const ImVec2 track_a(bar_x0, bar_y0);
        const ImVec2 track_b(bar_x0 + bar_area, bar_y0 + kBarHeight);
        draw->AddRectFilled(track_a, track_b, kTrackColour, kBarRound);

        if (z.seconds > 0.0) {
            const float len = static_cast<float>(z.seconds / total) * bar_area;
            draw->AddRectFilled(track_a,
                                ImVec2(bar_x0 + std::max(len, kBarRound * 2.0f),
                                       bar_y0 + kBarHeight),
                                zone_colour(i), kBarRound);
        }

        // Direct label: the time, and the share it represents.
        char value[64];
        std::snprintf(value, sizeof value, "%s   %4.1f %%",
                      io::format_duration(static_cast<Long>(z.seconds)).c_str(), pct);
        ImGui::SetCursorScreenPos(
            ImVec2(bar_x0 + bar_area + gap, row_min.y + text_dy));
        text_coloured(z.seconds > 0.0 ? kTextPrimary : kTextMuted, value);
    }

    // Claim the whole grid once, so anything drawn after the chart starts below.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(avail, row_h * static_cast<float>(table.zones.size())));
}

} // namespace gui
