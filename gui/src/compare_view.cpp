#include "compare_view.hpp"

#include "format.hpp"
#include "palette.hpp"   // series_colour
#include "theme.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gui {

namespace {

/// A little headroom so the peaks of a trace do not sit on the frame.
constexpr double kYPadFraction = 0.05;

int format_distance_tick(double value, char* buff, int size, void*) {
    return std::snprintf(buff, static_cast<std::size_t>(size), "%.4g km", value);
}

int format_time_tick(double value, char* buff, int size, void*) {
    const std::string s = fmt::elapsed_clock(value);
    return std::snprintf(buff, static_cast<std::size_t>(size), "%s", s.c_str());
}

/// A channel resampled onto whichever axis the comparison is using.
///
/// Distance is recorded per track point while a channel only has samples where
/// its field was present, so the two are not index-aligned; both are ascending
/// in time, which makes the lookup a merge rather than a search per sample.
struct Trace {
    std::vector<double> x, y;
};

Trace project(const channels::Channel& c, const DistanceAxis& dist, CompareAxis axis) {
    Trace t;
    t.x.reserve(c.t_s.size());
    t.y.reserve(c.t_s.size());

    if (axis == CompareAxis::Elapsed) {
        for (std::size_t i = 0; i < c.t_s.size(); ++i) {
            t.x.push_back(c.t_s[i]);
            t.y.push_back(c.value[i]);
        }
        return t;
    }

    if (dist.empty()) return t;

    std::size_t j = 0;   // walks the distance axis alongside the channel
    for (std::size_t i = 0; i < c.t_s.size(); ++i) {
        const double ts = c.t_s[i];
        while (j + 1 < dist.t_s.size() && dist.t_s[j + 1] < ts) ++j;
        if (j + 1 >= dist.t_s.size()) break;   // channel outlasts the distance

        const double t0 = dist.t_s[j], t1 = dist.t_s[j + 1];
        const double f  = (t1 > t0) ? std::clamp((ts - t0) / (t1 - t0), 0.0, 1.0) : 0.0;
        t.x.push_back(dist.km[j] + f * (dist.km[j + 1] - dist.km[j]));
        t.y.push_back(c.value[i]);
    }
    return t;
}

const channels::Channel* find(const CompareRide& ride, const std::string& name) {
    if (!ride.channels) return nullptr;
    for (const channels::Channel& c : *ride.channels)
        if (c.name == name) return &c;
    return nullptr;
}

/// Every ride's trace for one channel, plus who could not supply it.
struct ChannelTraces {
    std::vector<Trace>       traces;   ///< Parallel to `who`.
    std::vector<std::size_t> who;      ///< Index into the ride list.
    std::vector<std::string> missing;  ///< Labels of rides without the channel.
    std::string              unit;
};

ChannelTraces gather(const std::vector<CompareRide>& rides,
                     const std::string& name, CompareAxis axis) {
    ChannelTraces out;
    for (std::size_t r = 0; r < rides.size(); ++r) {
        const channels::Channel* c = find(rides[r], name);
        if (!c || c->t_s.size() < 2) { out.missing.push_back(rides[r].label); continue; }

        Trace t = project(*c, rides[r].distance ? *rides[r].distance : DistanceAxis{}, axis);
        if (t.x.size() < 2) { out.missing.push_back(rides[r].label); continue; }

        if (out.unit.empty()) out.unit = c->unit;
        out.traces.push_back(std::move(t));
        out.who.push_back(r);
    }
    return out;
}

Span x_extent(const ChannelTraces& ct) {
    Span s;
    bool any = false;
    for (const Trace& t : ct.traces) {
        if (t.x.empty()) continue;
        if (!any) { s.lo = t.x.front(); s.hi = t.x.back(); any = true; continue; }
        s.lo = std::min(s.lo, t.x.front());
        s.hi = std::max(s.hi, t.x.back());
    }
    return s;
}

Span y_extent(const ChannelTraces& ct) {
    Span s;
    bool any = false;
    for (const Trace& t : ct.traces)
        for (const double v : t.y) {
            if (!std::isfinite(v)) continue;
            if (!any) { s.lo = s.hi = v; any = true; continue; }
            s.lo = std::min(s.lo, v);
            s.hi = std::max(s.hi, v);
        }
    if (!any) return {0.0, 1.0};

    const double range = (s.hi - s.lo) > 0.0 ? (s.hi - s.lo)
                                             : std::max(1.0, std::abs(s.hi));
    return {s.lo - range * kYPadFraction, s.hi + range * kYPadFraction};
}

/// The whole comparison's x extent, so every plot shares one span.
Span overall_extent(const std::vector<CompareRide>& rides,
                    const std::vector<std::string>& names,
                    const std::vector<char>& selected, CompareAxis axis) {
    Span s;
    bool any = false;
    for (std::size_t n = 0; n < names.size(); ++n) {
        if (n >= selected.size() || !selected[n]) continue;
        const Span e = x_extent(gather(rides, names[n], axis));
        if (e.empty()) continue;
        if (!any) { s = e; any = true; continue; }
        s.lo = std::min(s.lo, e.lo);
        s.hi = std::max(s.hi, e.hi);
    }
    return s;
}

void draw_hover_readout(const ChannelTraces& ct,
                        const std::vector<CompareRide>& rides,
                        const std::string& unit, CompareAxis axis) {
    if (!ImPlot::IsPlotHovered()) return;

    const ImPlotPoint m = ImPlot::GetPlotMousePos();
    char head[96];
    if (axis == CompareAxis::Distance)
        std::snprintf(head, sizeof head, "at %.2f km", m.x);
    else
        std::snprintf(head, sizeof head, "at %s", fmt::elapsed_clock(m.x).c_str());

    if (!ImGui::BeginTooltip()) return;
    ImGui::TextUnformatted(head);
    ImGui::Separator();

    // Each ride's own value where the cursor is, rather than one reading: the
    // whole point is the gap between them.
    for (std::size_t k = 0; k < ct.traces.size(); ++k) {
        const Trace& t = ct.traces[k];
        const auto   it = std::lower_bound(t.x.begin(), t.x.end(), m.x);
        if (it == t.x.end()) continue;
        const std::size_t i = static_cast<std::size_t>(it - t.x.begin());

        char row[160];
        std::snprintf(row, sizeof row, "%.4g %s   %s", t.y[i], unit.c_str(),
                      rides[ct.who[k]].label.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, series_colour(ct.who[k]));
        ImGui::TextUnformatted(row);
        ImGui::PopStyleColor();
    }
    ImGui::EndTooltip();
}

} // namespace

std::vector<std::string> compare_channels(const std::vector<CompareRide>& rides) {
    std::vector<std::string> names;
    for (const CompareRide& r : rides) {
        if (!r.channels) continue;
        for (const channels::Channel& c : *r.channels)
            if (std::find(names.begin(), names.end(), c.name) == names.end())
                names.push_back(c.name);
    }
    return names;
}

void draw_compare_plots(const std::vector<CompareRide>& rides,
                        const std::vector<std::string>& names,
                        const std::vector<char>& selected,
                        CompareAxis axis, Span& range,
                        float width, float height) {
    if (rides.empty() || names.empty()) return;

    const Span span = overall_extent(rides, names, selected, axis);
    if (span.empty()) return;
    if (range.empty()) range = span;

    for (std::size_t n = 0; n < names.size(); ++n) {
        if (n >= selected.size() || !selected[n]) continue;

        const ChannelTraces ct = gather(rides, names[n], axis);

        std::string caption = names[n];
        if (!ct.unit.empty()) caption += "  -  " + ct.unit;
        if (!ct.missing.empty()) {
            caption += ",  not recorded by: ";
            for (std::size_t i = 0; i < ct.missing.size(); ++i)
                caption += (i ? ", " : "") + ct.missing[i];
        }
        theme::text_coloured(theme::kTextSecondary, caption);

        if (ct.traces.empty()) {
            theme::text_coloured(theme::kTextMuted, "  (no ride recorded this)");
            continue;
        }

        const std::string id = "##cmp_" + names[n];
        const Span        y  = y_extent(ct);

        const theme::PlotStyleScope plot_style;
        if (ImPlot::BeginPlot(id.c_str(), ImVec2(width, height),
                              ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes(axis == CompareAxis::Distance ? "distance" : "elapsed time",
                              ct.unit.c_str());
            ImPlot::SetupAxisLinks(ImAxis_X1, &range.lo, &range.hi);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, span.lo, span.hi);
            ImPlot::SetupAxisFormat(ImAxis_X1, axis == CompareAxis::Distance
                                                   ? format_distance_tick
                                                   : format_time_tick);
            ImPlot::SetupAxisLimits(ImAxis_Y1, y.lo, y.hi, ImPlotCond_Always);

            for (std::size_t k = 0; k < ct.traces.size(); ++k) {
                const Trace& t = ct.traces[k];
                ImPlotSpec line;
                line.LineColor  = series_colour(ct.who[k]);
                line.LineWeight = 1.5f;
                ImPlot::PlotLine(rides[ct.who[k]].label.c_str(), t.x.data(),
                                 t.y.data(), static_cast<int>(t.x.size()), line);
            }

            draw_hover_readout(ct, rides, ct.unit, axis);
            ImPlot::EndPlot();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }
}

} // namespace gui
