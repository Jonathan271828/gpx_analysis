#include "peaks_chart.hpp"

#include "bar_row.hpp"
#include "format.hpp"
#include "io_base.hpp"   // io::format_duration
#include "palette.hpp"
#include "theme.hpp"   // zone_colour, zone_label

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

namespace {

using theme::kTextPrimary;
using theme::kTextSecondary;

// "Z4 Threshold" -> "Z4"
std::string short_tag(const std::string& label) {
    const std::string::size_type sp = label.find(' ');
    return (sp == std::string::npos) ? label : label.substr(0, sp);
}

// --- hold curve ------------------------------------------------------------

// A single series, so it takes the first categorical slot: 4.79:1 on this
// surface, clear of the 3:1 floor for marks.
using theme::kCurve;

constexpr float kCurveHeight = 190.0f;

} // namespace

float peaks_chart_height(const std::vector<PeakBar>& peaks) {
    return bar::chart_height(peaks.empty() ? 1 : peaks.size());
}

void draw_peaks_chart(const std::vector<PeakBar>& peaks,
                      const zones::ZoneTable& zt) {
    if (peaks.empty()) {
        theme::text_coloured(kTextSecondary, "No peak efforts for this track.");
        return;
    }

    // --- Heading -------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::TextUnformatted("Peak power efforts");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    theme::text_coloured(kTextSecondary,
                  peaks.front().measured
                      ? "  measured  -  best average power per duration"
                      : "  estimated  -  best average power per duration");
    ImGui::Dummy(ImVec2(0.0f, bar::kHeadingGap));

    // --- Column geometry -----------------------------------------------------
    float dur_w = 0.0f;
    for (const PeakBar& p : peaks)
        dur_w = std::max(dur_w,
                         ImGui::CalcTextSize(io::format_duration(p.duration_s).c_str()).x);

    const float watt_w = ImGui::CalcTextSize("8888 W").x;
    const float wkg_w  = ImGui::CalcTextSize("88.8 W/kg").x;
    const float zone_w = ImGui::CalcTextSize("Z8").x;
    const float at_w   = ImGui::CalcTextSize("at 00h 00m 00s").x;
    const float gap    = ImGui::GetStyle().ItemSpacing.x;
    const float avail  = ImGui::GetContentRegionAvail().x;
    const float bar_area = std::max(60.0f, avail - dur_w - watt_w - wkg_w -
                                              zone_w - at_w - 6.0f * gap);

    // Bars run from zero against the ride's best effort, so their lengths are
    // proportional to the watts printed beside them.
    Real peak = 0.0;
    for (const PeakBar& p : peaks) peak = std::max(peak, p.avg_power_w);
    if (peak <= 0.0) peak = 1.0;

    ImDrawList* draw  = ImGui::GetWindowDrawList();
    const float row_h = bar::row_height();

    // Rows are positioned by hand at exactly row_h apart, so the height that
    // peaks_chart_height() reports stays exact.
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    for (std::size_t i = 0; i < peaks.size(); ++i) {
        const PeakBar& p = peaks[i];

        const ImVec2 row_min(origin.x, origin.y + row_h * static_cast<float>(i));
        const ImVec2 row_max(row_min.x + avail, row_min.y + row_h);
        const float  text_dy = (row_h - ImGui::GetTextLineHeight()) * 0.5f;

        const std::string zname = zone_label(zt, p.zone);

        if (ImGui::IsWindowHovered() &&
            ImGui::IsMouseHoveringRect(row_min, row_max)) {
            bar::highlight_row(draw, row_min, row_max);
            ImGui::SetTooltip("best %s effort\n%.0f W  (%.2f W/kg)%s%s\nstarting at %s",
                              io::format_duration(p.duration_s).c_str(),
                              p.avg_power_w, p.wkg,
                              zname.empty() ? "" : "\n",
                              zname.empty() ? "" : zname.c_str(),
                              io::format_duration(p.start_offset_s).c_str());
        }

        // Duration, right-aligned so the column reads as a scale.
        const std::string dur = io::format_duration(p.duration_s);
        ImGui::SetCursorScreenPos(
            ImVec2(row_min.x + dur_w - ImGui::CalcTextSize(dur.c_str()).x,
                   row_min.y + text_dy));
        theme::text_coloured(kTextPrimary, dur);

        // The bar.
        const float  bar_x0 = row_min.x + dur_w + gap;
        const float  bar_y0 = row_min.y + (row_h - bar::kBarHeight) * 0.5f;
        bar::draw_bar(draw, ImVec2(bar_x0, bar_y0), bar_area,
                      static_cast<float>(p.avg_power_w / peak),
                      p.zone >= 0 ? zone_colour(static_cast<std::size_t>(p.zone))
                                  : ImGui::GetColorU32(theme::kNoZone));

        // Direct labels: watts, per-kilo, the zone by name, and where it happened.
        float x = bar_x0 + bar_area + gap;
        char  buf[64];

        std::snprintf(buf, sizeof buf, "%4.0f W", p.avg_power_w);
        ImGui::SetCursorScreenPos(ImVec2(x, row_min.y + text_dy));
        theme::text_coloured(kTextPrimary, buf);
        x += watt_w + gap;

        std::snprintf(buf, sizeof buf, "%.1f W/kg", p.wkg);
        ImGui::SetCursorScreenPos(ImVec2(x, row_min.y + text_dy));
        theme::text_coloured(kTextSecondary, p.wkg > 0.0 ? buf : "");
        x += wkg_w + gap;

        // The zone tag repeats what the bar's colour says, so the reading never
        // depends on colour alone.
        ImGui::SetCursorScreenPos(ImVec2(x, row_min.y + text_dy));
        theme::text_coloured(kTextPrimary, short_tag(zname));
        x += zone_w + gap;

        ImGui::SetCursorScreenPos(ImVec2(x, row_min.y + text_dy));
        theme::text_coloured(kTextSecondary,
                      "at " + io::format_duration(p.start_offset_s));
    }

    // Claim the whole grid once, so anything after the chart starts below it.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(avail, row_h * static_cast<float>(peaks.size())));
}

float hold_curve_height() {
    return kCurveHeight;
}

void draw_hold_curve(const std::vector<PeakBar>& peaks, Real ftp_w,
                     HoldRef ref, float width) {
    if (peaks.size() < 2) return;

    const bool use_ftp = (ref == HoldRef::Ftp) && ftp_w > 0.0;

    Real basis = 0.0;
    for (const PeakBar& p : peaks) basis = std::max(basis, p.avg_power_w);
    if (use_ftp) basis = ftp_w;
    if (basis <= 0.0) return;

    // x is the duration held, y the fraction of the reference power.
    std::vector<double> xs, ys, ticks;
    std::vector<std::string> labels;
    xs.reserve(peaks.size());
    ys.reserve(peaks.size());
    for (const PeakBar& p : peaks) {
        xs.push_back(static_cast<double>(p.duration_s));
        ys.push_back(p.avg_power_w / basis * 100.0);
        ticks.push_back(static_cast<double>(p.duration_s));
        labels.push_back(fmt::compact_duration(p.duration_s));
    }

    std::vector<const char*> label_ptrs;
    label_ptrs.reserve(labels.size());
    for (const std::string& s : labels) label_ptrs.push_back(s.c_str());

    const auto  my   = std::minmax_element(ys.begin(), ys.end());
    const double y_lo = std::min(0.0, *my.first - 5.0);
    const double y_hi = *my.second + 8.0;

    const theme::PlotStyleScope plot_style;

    // The id carries the reference, so switching it starts from limits that fit
    // the new y range rather than keeping the old one's.
    const char* id = use_ftp ? "##hold_ftp" : "##hold_peak";

    if (ImPlot::BeginPlot(id, ImVec2(width, kCurveHeight),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                          ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("duration held",
                          use_ftp ? "% of FTP" : "% of best effort");
        // Log x: these windows span 5 s to an hour, so a linear axis would crowd
        // everything below 10 minutes into the left edge.
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(),
                               static_cast<int>(ticks.size()), label_ptrs.data());
        ImPlot::SetupAxisLimits(ImAxis_X1, xs.front() * 0.8, xs.back() * 1.25,
                                ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, y_lo, y_hi, ImPlotCond_Once);

        // 100 % is the reference itself: the ride's best effort, or FTP.
        const double hundred = 100.0;
        ImPlotSpec   refline;
        refline.LineColor  = ImVec4(1.0f, 1.0f, 1.0f, 0.28f);
        refline.LineWeight = 1.0f;
        refline.Flags      = ImPlotInfLinesFlags_Horizontal;
        ImPlot::PlotInfLines("##ref", &hundred, 1, refline);

        ImPlotSpec line;
        line.LineColor  = kCurve;
        line.LineWeight = 2.0f;
        line.Marker     = ImPlotMarker_Circle;
        line.MarkerSize = 4.0f;
        ImPlot::PlotLine("hold", xs.data(), ys.data(),
                         static_cast<int>(xs.size()), line);

        // Readout at the nearest effort.
        if (ImPlot::IsPlotHovered()) {
            const ImPlotPoint m = ImPlot::GetPlotMousePos();
            std::size_t k = 0;
            double best = 1e300;
            for (std::size_t i = 0; i < xs.size(); ++i) {
                // Compare in log space, matching what the eye sees on this axis.
                const double d = std::abs(std::log10(xs[i]) - std::log10(std::max(m.x, 1e-6)));
                if (d < best) { best = d; k = i; }
            }
            const ImVec2 pt = ImPlot::PlotToPixels(xs[k], ys[k]);
            ImPlot::PushPlotClipRect();
            ImPlot::GetPlotDrawList()->AddCircle(pt, 7.0f,
                                                ImGui::GetColorU32(kCurve), 0, 2.0f);
            ImPlot::PopPlotClipRect();
            ImGui::SetTooltip("held %.0f %% of %s\nfor %s   (%.0f W)",
                              ys[k], use_ftp ? "FTP" : "the best effort",
                              io::format_duration(peaks[k].duration_s).c_str(),
                              peaks[k].avg_power_w);
        }

        ImPlot::EndPlot();
    }

}

} // namespace gui
