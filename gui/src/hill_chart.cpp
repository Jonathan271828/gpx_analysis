#include "hill_chart.hpp"

#include "io_base.hpp"   // io::format_duration
#include "palette.hpp"   // zone_colour

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace gui {

namespace {

// The profile outline. The area beneath it is coloured by power zone, so the
// outline itself stays neutral rather than competing with Z1's blue.
const ImVec4 kOutline  = ImVec4(1.000f, 1.000f, 1.000f, 0.60f);
const ImVec4 kNoZone   = ImVec4(0.600f, 0.600f, 0.580f, 1.00f);  // power unknown
const ImVec4 kCurve    = ImVec4(0.224f, 0.529f, 0.898f, 1.00f);  // hover marker

const ImVec4 kTextPrimary   = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);  // #ffffff
const ImVec4 kTextSecondary = ImVec4(0.765f, 0.761f, 0.718f, 1.00f);  // #c3c2b7
const ImVec4 kSurface       = ImVec4(0.102f, 0.102f, 0.098f, 1.00f);  // #1a1a19
const ImVec4 kGrid          = ImVec4(1.000f, 1.000f, 1.000f, 0.09f);  // recessive

constexpr float kPlotHeight = 150.0f;

// The fills carry meaning, so they are drawn opaque: the palette is validated
// against this surface at full strength, and blending it toward the background
// breaks that. Measured at 70 % alpha, the effective colours fail outright --
// aqua drops under the chroma floor and reads grey, green leaves the lightness
// band, and four of the seven fall below 3:1 contrast.
constexpr float kFillAlpha  = 1.0f;

// "2026-08-10T16:05:58.000Z" -> "16:05:58". The date is already in the table
// above, so only the clock time earns space in the heading.
std::string clock_time(const std::string& iso) {
    const std::string::size_type t = iso.find('T');
    if (t == std::string::npos || t + 9 > iso.size()) return iso;
    return iso.substr(t + 1, 8);
}

// "8.75 km  +205.8 m  2.4 %  178 W  Cat 3  32m 28s"
std::string subtitle(const HillProfile& h) {
    char buf[192];
    std::string cat = h.category.empty() ? std::string() : "  Cat " + h.category;
    if (h.has_power)
        std::snprintf(buf, sizeof buf, "%.2f km   +%.1f m   %.1f %%   %.0f W   %.0f m/h%s",
                      h.distance_km, h.gain_m, h.avg_grade_pct, h.avg_power_w,
                      h.vam_mh, cat.c_str());
    else
        std::snprintf(buf, sizeof buf, "%.2f km   +%.1f m   %.1f %%   %.0f m/h%s",
                      h.distance_km, h.gain_m, h.avg_grade_pct, h.vam_mh, cat.c_str());
    std::string s = buf;
    if (h.duration_s > 0)         s += "   " + io::format_duration(h.duration_s);
    if (!h.start_time.empty())    s += "   from " + clock_time(h.start_time);
    return s;
}

// Vertical marker plus a readout at the sample nearest the cursor, naming the
// stretch it falls in and the training zone it was ridden in.
void hover_readout(const HillProfile& hill, const zones::ZoneTable& zt,
                   const std::vector<Real>& xs, const char* x_unit) {
    if (!ImPlot::IsPlotHovered() || xs.empty()) return;

    const std::vector<Real>& ys    = hill.ele_m;
    const ImPlotPoint        mouse = ImPlot::GetPlotMousePos();

    // Both axes are monotone in x, so the nearest sample is one of the two
    // straddling the cursor.
    std::size_t k = static_cast<std::size_t>(
        std::lower_bound(xs.begin(), xs.end(), mouse.x) - xs.begin());
    if (k >= xs.size()) k = xs.size() - 1;
    if (k > 0 && (mouse.x - xs[k - 1]) < (xs[k] - mouse.x)) --k;

    const ImVec2 pt  = ImPlot::PlotToPixels(xs[k], ys[k]);
    const ImVec2 pos = ImPlot::GetPlotPos();
    const ImVec2 sz  = ImPlot::GetPlotSize();

    ImDrawList* dl = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    dl->AddLine(ImVec2(pt.x, pos.y), ImVec2(pt.x, pos.y + sz.y),
                IM_COL32(255, 255, 255, 60), 1.0f);
    dl->AddCircleFilled(pt, 4.0f, ImGui::GetColorU32(kCurve));
    ImPlot::PopPlotClipRect();

    const HillSegment* seg = nullptr;
    for (const HillSegment& s : hill.segments)
        if (k >= s.begin && k <= s.end) { seg = &s; break; }

    if (!ImGui::BeginTooltip()) return;

    ImGui::Text("%.0f m elevation", ys[k]);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
    ImGui::Text("at %.2f %s", xs[k], x_unit);
    ImGui::PopStyleColor();

    if (seg) {
        ImGui::Separator();

        // Name the zone rather than leaving the colour to be remembered, with
        // its swatch alongside so the two are tied together.
        const std::string zname = zone_label(zt, seg->zone);
        if (!zname.empty()) {
            zone_swatch(static_cast<std::size_t>(seg->zone));
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextUnformatted(zname.c_str());
        }

        if (seg->has_power) ImGui::Text("%.0f W average", seg->avg_power_w);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
        ImGui::Text("section: %.1f %% over %.0f m", seg->avg_grade_pct, seg->dist_m);
        ImGui::PopStyleColor();
    }

    ImGui::EndTooltip();
}

} // namespace

float hill_chart_height() {
    return ImGui::GetTextLineHeight() +
           ImGui::GetStyle().ItemSpacing.y * 2.0f + kPlotHeight;
}

void draw_hill_chart(const HillProfile& hill, int index, HillAxis axis,
                     const zones::ZoneTable& zt, float width) {
    // Heading: which climb, and the same numbers as its row in the table.
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::Text("Hill %d", index + 1);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
    ImGui::Text("   %s", subtitle(hill).c_str());
    ImGui::PopStyleColor();

    if (hill.ele_m.size() < 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
        ImGui::TextUnformatted("  (no elevation samples for this climb)");
        ImGui::PopStyleColor();
        return;
    }

    // Time is only offered when the climb carries usable timestamps.
    const bool use_time = (axis == HillAxis::Time) && !hill.time_min.empty();
    const std::vector<Real>& xs = use_time ? hill.time_min : hill.dist_km;
    const char* x_label = use_time ? "time (min)" : "distance (km)";
    const char* x_unit  = use_time ? "min" : "km";

    const auto  mm  = std::minmax_element(hill.ele_m.begin(), hill.ele_m.end());
    const Real  lo  = *mm.first;
    const Real  hi  = *mm.second;
    const Real  pad = std::max(5.0, (hi - lo) * 0.12);

    // The plot id carries the axis mode, so switching axes starts from limits
    // that fit the new x range instead of keeping the old one's.
    char id[64];
    std::snprintf(id, sizeof id, "##hill%d_%s", index, use_time ? "t" : "d");

    ImPlot::PushStyleColor(ImPlotCol_FrameBg,  kSurface);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,   kSurface);
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, kGrid);

    if (ImPlot::BeginPlot(id, ImVec2(width, kPlotHeight),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                          ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes(x_label, "elevation (m)");
        ImPlot::SetupAxisLimits(ImAxis_X1, xs.front(), xs.back(), ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, lo - pad, hi + pad, ImPlotCond_Once);

        // ImPlot 1.0 takes per-item styling as an ImPlotSpec rather than the
        // SetNext*Style calls it obsoleted.
        const int  n    = static_cast<int>(xs.size());
        const Real base = lo - pad;

        // One shaded run per constant-gradient stretch, coloured by the power
        // zone it was ridden in. Consecutive stretches share a boundary sample,
        // so the fills meet exactly.
        if (hill.segments.empty()) {
            ImPlotSpec fill;
            fill.FillColor = kNoZone;
            fill.FillAlpha = kFillAlpha;
            ImPlot::PlotShaded("##fill", xs.data(), hill.ele_m.data(), n, base, fill);
        } else {
            for (std::size_t s = 0; s < hill.segments.size(); ++s) {
                const HillSegment& seg = hill.segments[s];
                const int          cnt = static_cast<int>(seg.end - seg.begin + 1);
                if (cnt < 2) continue;

                ImPlotSpec fill;
                fill.FillColor = (seg.zone >= 0)
                                     ? zone_colour_vec(static_cast<std::size_t>(seg.zone))
                                     : kNoZone;
                fill.FillAlpha = kFillAlpha;

                char sid[32];
                std::snprintf(sid, sizeof sid, "##seg%zu", s);
                ImPlot::PlotShaded(sid, xs.data() + seg.begin,
                                   hill.ele_m.data() + seg.begin, cnt, base, fill);
            }

            // A hairline where the gradient changes, so touching stretches of
            // similar hue still read as separate sections.
            ImPlot::PushPlotClipRect();
            ImDrawList* dl = ImPlot::GetPlotDrawList();
            for (std::size_t s = 1; s < hill.segments.size(); ++s) {
                const std::size_t k = hill.segments[s].begin;
                const ImVec2 top = ImPlot::PlotToPixels(xs[k], hill.ele_m[k]);
                const ImVec2 bot = ImPlot::PlotToPixels(xs[k], base);
                dl->AddLine(top, bot, IM_COL32(20, 20, 20, 140), 1.0f);
            }
            ImPlot::PopPlotClipRect();
        }

        ImPlotSpec line;
        line.LineColor  = kOutline;
        line.LineWeight = 2.0f;
        ImPlot::PlotLine("##outline", xs.data(), hill.ele_m.data(), n, line);

        hover_readout(hill, zt, xs, x_unit);
        ImPlot::EndPlot();
    }

    ImPlot::PopStyleColor(3);
}

} // namespace gui
