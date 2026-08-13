#include "signal_view.hpp"

#include "format.hpp"
#include "palette.hpp"   // series_colour
#include "theme.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace gui {

namespace {

/// A little headroom so the peaks of a trace do not sit on the frame.
constexpr double kYPadFraction = 0.05;

/// Ticks read as elapsed clock time rather than a raw second count: a ride is
/// navigated by "twenty minutes in", not by "second 1200".
int format_time_tick(double value, char* buff, int size, void*) {
    const std::string s = fmt::elapsed_clock(value);
    return std::snprintf(buff, static_cast<std::size_t>(size), "%s", s.c_str());
}

struct Extent {
    double lo = 0.0, hi = 1.0;
};

Extent value_extent(const std::vector<Real>& v) {
    Extent e;
    bool   any = false;
    for (const Real x : v) {
        if (!std::isfinite(x)) continue;
        if (!any) { e.lo = e.hi = x; any = true; continue; }
        e.lo = std::min(e.lo, static_cast<double>(x));
        e.hi = std::max(e.hi, static_cast<double>(x));
    }
    if (!any) return e;

    // A constant trace would otherwise collapse to a zero-height axis.
    const double span = (e.hi - e.lo) > 0.0 ? (e.hi - e.lo) : std::max(1.0, std::abs(e.hi));
    e.lo -= span * kYPadFraction;
    e.hi += span * kYPadFraction;
    return e;
}

/// The widest time span across every selected channel.
TimeRange full_span(const std::vector<channels::Channel>& channels,
                    const std::vector<char>& selected) {
    TimeRange r;
    bool any = false;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        if (i >= selected.size() || !selected[i] || channels[i].t_s.empty()) continue;
        const double first = channels[i].t_s.front();
        const double last  = channels[i].t_s.back();
        if (!any) { r.begin_s = first; r.end_s = last; any = true; continue; }
        r.begin_s = std::min(r.begin_s, first);
        r.end_s   = std::max(r.end_s, last);
    }
    if (any && r.end_s <= r.begin_s) r.end_s = r.begin_s + 1.0;
    return r;
}

void draw_hover_readout(const channels::Channel& channel) {
    if (!ImPlot::IsPlotHovered()) return;

    const ImPlotPoint m = ImPlot::GetPlotMousePos();
    char primary[96], secondary[96];
    std::snprintf(primary, sizeof primary, "%.4g %s", m.y, channel.unit.c_str());
    std::snprintf(secondary, sizeof secondary, "at %s",
                  fmt::elapsed_clock(m.x).c_str());
    theme::value_tooltip(primary, secondary);
}

} // namespace

void draw_signal_plots(const std::vector<channels::Channel>& channels,
                       const std::vector<char>& selected,
                       TimeRange& range, float width, float height) {
    const TimeRange span = full_span(channels, selected);
    if (span.empty()) return;
    if (range.empty()) range = span;

    for (std::size_t i = 0; i < channels.size(); ++i) {
        if (i >= selected.size() || !selected[i]) continue;

        const channels::Channel& c = channels[i];
        if (c.t_s.size() < 2) continue;

        theme::text_coloured(theme::kTextSecondary,
                             c.name + "  -  " + c.unit + ",  " +
                                 std::to_string(c.t_s.size()) + " samples");

        const std::string id = "##signal_" + c.name;
        const Extent      y  = value_extent(c.value);

        const theme::PlotStyleScope plot_style;
        if (ImPlot::BeginPlot(id.c_str(), ImVec2(width, height),
                              ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                              ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes("elapsed time", c.unit.c_str());

            // Every plot reads and writes the same two doubles, so panning or
            // zooming one moves all of them, and the stack stays aligned.
            ImPlot::SetupAxisLinks(ImAxis_X1, &range.begin_s, &range.end_s);

            // Pinned to the ride: without this the view pans off into empty
            // time either side, where every tick reads 0:00 and there is
            // nothing to see.
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, span.begin_s, span.end_s);
            ImPlot::SetupAxisFormat(ImAxis_X1, format_time_tick);
            ImPlot::SetupAxisLimits(ImAxis_Y1, y.lo, y.hi, ImPlotCond_Always);

            ImPlotSpec line;
            line.LineColor  = series_colour(i);
            line.LineWeight = 1.5f;
            ImPlot::PlotLine(c.name.c_str(), c.t_s.data(), c.value.data(),
                             static_cast<int>(c.t_s.size()), line);

            draw_hover_readout(c);
            ImPlot::EndPlot();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }
}

} // namespace gui
