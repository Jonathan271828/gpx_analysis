#include "durability_chart.hpp"

#include "format.hpp"
#include "span.hpp"
#include "palette.hpp"   // series_colour
#include "theme.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

namespace {

constexpr float kPlotHeight = 210.0f;
constexpr float kHeadingGap = 6.0f;

/// One duration's readings, dropped where the ride never got that deep.
struct Line {
    std::vector<double> kj, watts;
    std::string         label;
    Real                fade_pct = 0.0;
    bool                valid    = false;
};

Line build(const durability::Curve& c) {
    Line l;
    l.label    = fmt::compact_duration(c.duration_s);
    l.fade_pct = c.fade_pct;
    l.valid    = c.valid;
    for (const durability::Effort& e : c.efforts) {
        if (!e.found) continue;
        l.kj.push_back(e.after_kj);
        l.watts.push_back(e.avg_power_w);
    }
    return l;
}

} // namespace

float durability_chart_height() {
    return ImGui::GetTextLineHeight() * 2.0f + kHeadingGap + kPlotHeight;
}

void draw_durability_chart(const durability::Report& report, float width) {
    if (!report.valid) return;

    std::vector<Line> lines;
    for (const durability::Curve& c : report.curves) {
        Line l = build(c);
        if (l.kj.size() >= 2) lines.push_back(std::move(l));
    }
    if (lines.empty()) return;

    char heading[192];
    std::snprintf(heading, sizeof heading,
                  "Fatigue resistance   %s   -   %.0f kJ over the ride",
                  report.measured ? "measured" : "estimated", report.total_kj);
    theme::text_coloured(theme::kTextPrimary, heading);

    // The fade is the number the chart exists to show, so it is stated rather
    // than left to be read off the slope.
    std::string fades;
    for (const Line& l : lines) {
        if (!l.valid) continue;
        char one[64];
        std::snprintf(one, sizeof one, "%s %+.1f %%", l.label.c_str(), l.fade_pct);
        fades += (fades.empty() ? "fade:  " : "     ") + std::string(one);
    }
    theme::text_coloured(theme::kTextSecondary, fades);
    ImGui::Dummy(ImVec2(0.0f, kHeadingGap));

    // Ranges are set rather than fitted: ImPlot pads a fit by nothing, so the
    // highest reading would sit exactly on the frame and read as clipped.
    Span x, y;
    for (const Line& l : lines) {
        for (const double kj : l.kj)     x.include(kj);
        for (const double w  : l.watts)  y.include(w);
    }
    y = y.padded(0.12);

    const theme::PlotStyleScope plot_style;
    if (ImPlot::BeginPlot("##durability", ImVec2(width, kPlotHeight),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("work already done (kJ)", "best power (W)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, x.hi * 1.02, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, y.lo, y.hi, ImPlotCond_Always);

        for (std::size_t i = 0; i < lines.size(); ++i) {
            ImPlotSpec line;
            line.LineColor       = series_colour(i);
            line.LineWeight      = 2.0f;
            line.Marker          = ImPlotMarker_Circle;
            line.MarkerSize      = 3.5f;
            line.MarkerFillColor = series_colour(i);
            ImPlot::PlotLine(lines[i].label.c_str(), lines[i].kj.data(),
                             lines[i].watts.data(),
                             static_cast<int>(lines[i].kj.size()), line);
        }
        ImPlot::EndPlot();
    }
}

} // namespace gui
