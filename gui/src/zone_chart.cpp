#include "zone_chart.hpp"

#include "bar_row.hpp"
#include "io_base.hpp"   // io::format_duration
#include "palette.hpp"   // zone_colour, zone_bounds_text
#include "theme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>

namespace gui {

namespace {

using theme::kTextMuted;
using theme::kTextPrimary;
using theme::kTextSecondary;

/// Where each column of a row starts, measured once for the whole chart so the
/// columns line up down the table instead of jostling per row.
struct RowLayout {
    float swatch_w = 0.0f;  ///< Side of the square colour swatch.
    float label_w  = 0.0f;  ///< Widest zone name.
    float bounds_w = 0.0f;  ///< Widest bounds string.
    float bar_area = 0.0f;  ///< Track width.
    float gap      = 0.0f;  ///< One ItemSpacing.x.
    float avail    = 0.0f;  ///< Full content width.

    float label_x()  const { return swatch_w + gap; }
    float bounds_x() const { return swatch_w + label_w + 2.0f * gap; }
    float bar_x()    const { return swatch_w + label_w + bounds_w + 3.0f * gap; }
    float value_x()  const { return bar_x() + bar_area + gap; }
};

RowLayout measure(const zones::ZoneTable& table, const std::string& unit) {
    RowLayout l;
    l.swatch_w = ImGui::GetTextLineHeight() * 0.42f;
    for (const zones::Zone& z : table.zones) {
        l.label_w  = std::max(l.label_w, ImGui::CalcTextSize(z.label.c_str()).x);
        l.bounds_w = std::max(l.bounds_w,
                              ImGui::CalcTextSize(zone_bounds_text(z, unit).c_str()).x);
    }
    const float value_w = ImGui::CalcTextSize("00h 00m 00s   100.0 %").x;
    l.gap   = ImGui::GetStyle().ItemSpacing.x;
    l.avail = ImGui::GetContentRegionAvail().x;
    l.bar_area = std::max(60.0f, l.avail - l.swatch_w - l.label_w - l.bounds_w -
                                     value_w - 4.0f * l.gap);
    return l;
}

/// What is being measured, against what reference, over how long.
void draw_heading(const zones::ZoneTable& table) {
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::Text("Time in %s zones", table.kind.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    theme::text_coloured(kTextSecondary,
                         "  " + table.basis + "  -  total " +
                             io::format_duration(static_cast<Long>(table.total_s)));
    ImGui::Dummy(ImVec2(0.0f, bar::kHeadingGap));
}

void show_row_tooltip(const zones::Zone& z, const std::string& unit,
                      Real pct, Real total_s) {
    ImGui::SetTooltip("%s\n%s\n%s  (%.1f %% of %s)",
                      z.label.c_str(), zone_bounds_text(z, unit).c_str(),
                      io::format_duration(static_cast<Long>(z.seconds)).c_str(),
                      pct, io::format_duration(static_cast<Long>(total_s)).c_str());
}

/// The time in the zone, and the share of the ride it represents.
std::string value_text(const zones::Zone& z, Real pct) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s   %4.1f %%",
                  io::format_duration(static_cast<Long>(z.seconds)).c_str(), pct);
    return buf;
}

void draw_row(const zones::ZoneTable& table, Size index, const RowLayout& l,
              const std::string& unit, const ImVec2& row_min, float row_h) {
    const zones::Zone& z = table.zones[index];
    const Real pct = table.total_s > 0.0 ? 100.0 * z.seconds / table.total_s : 0.0;

    ImDrawList*  draw    = ImGui::GetWindowDrawList();
    const ImVec2 row_max(row_min.x + l.avail, row_min.y + row_h);

    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(row_min, row_max)) {
        bar::highlight_row(draw, row_min, row_max);
        show_row_tooltip(z, unit, pct, table.total_s);
    }

    // A swatch keeps the colour-to-zone mapping legible even for a zone with no
    // time in it, which has no bar to carry its colour.
    const float sw_y = row_min.y + (row_h - l.swatch_w) * 0.5f;
    draw->AddRectFilled(ImVec2(row_min.x, sw_y),
                        ImVec2(row_min.x + l.swatch_w, sw_y + l.swatch_w),
                        zone_colour(index), 2.0f);

    const float text_dy = (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    const float text_y  = row_min.y + text_dy;
    bar::text_at(ImVec2(row_min.x + l.label_x(),  text_y), kTextPrimary, z.label);
    bar::text_at(ImVec2(row_min.x + l.bounds_x(), text_y), kTextSecondary,
                 zone_bounds_text(z, unit));

    // Bar length is the zone's share of the total, so the track behind it reads
    // as 100 % and each bar agrees with the percentage printed beside it.
    const Real  total    = table.total_s > 0.0 ? table.total_s : 1.0;
    const float fraction = static_cast<float>(z.seconds / total);
    bar::draw_bar(draw,
                  ImVec2(row_min.x + l.bar_x(), row_min.y + (row_h - bar::kBarHeight) * 0.5f),
                  l.bar_area, fraction, zone_colour(index));

    bar::text_at(ImVec2(row_min.x + l.value_x(), text_y),
                 z.seconds > 0.0 ? kTextPrimary : kTextMuted, value_text(z, pct));
}

} // namespace

float zone_chart_height(const zones::ZoneTable& table) {
    const std::size_t rows =
        (table.valid && !table.zones.empty()) ? table.zones.size() : 1;
    return bar::chart_height(rows);
}

void draw_zone_chart(const zones::ZoneTable& table) {
    if (!table.valid || table.zones.empty()) {
        theme::text_coloured(kTextMuted,
                             "No zone distribution for this track (needs power "
                             "data and an FTP setting).");
        return;
    }

    const std::string unit = (table.kind == "power") ? "W" : "bpm";

    draw_heading(table);
    const RowLayout l = measure(table, unit);

    // Rows are placed by hand at exactly row_h apart. Letting ImGui advance the
    // cursor per row would add an ItemSpacing.y each time, which the height
    // zone_chart_height() reports would not account for.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  row_h  = bar::row_height();

    for (Size i = 0; i < table.zones.size(); ++i) {
        const ImVec2 row_min(origin.x, origin.y + row_h * static_cast<float>(i));
        draw_row(table, i, l, unit, row_min, row_h);
    }

    // Claim the whole grid once, so anything drawn after the chart starts below.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(l.avail, row_h * static_cast<float>(table.zones.size())));
}

} // namespace gui
