#include "profile_chart.hpp"

#include "bar_row.hpp"
#include "theme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

namespace {

using theme::kTextMuted;
using theme::kTextPrimary;
using theme::kStrength;
using theme::kTextSecondary;
using theme::kWeakness;

/// The scale runs to this departure either way, so a modest tilt is visible
/// without a large one running off the end.
constexpr Real  kFullScale = 0.40;
constexpr float kLabelW    = 60.0f;
constexpr float kValueW    = 190.0f;


const char* label_for(Long duration_s) {
    switch (duration_s) {
        case 5:    return "5 s";
        case 60:   return "1 min";
        case 300:  return "5 min";
        case 1200: return "20 min";
        default:   break;
    }
    return "";
}

/// What each duration is actually testing, so the row means something to a
/// reader who does not already know why these four were chosen.
const char* system_for(Long duration_s) {
    switch (duration_s) {
        case 5:    return "neuromuscular";
        case 60:   return "anaerobic";
        case 300:  return "aerobic power";
        case 1200: return "threshold";
        default:   break;
    }
    return "";
}

} // namespace

float profile_chart_height(const profile::Profile& p) {
    if (!p.valid) return 0.0f;

    const float line = ImGui::GetTextLineHeightWithSpacing();
    float       h    = line * 2.0f                       // heading + verdict
                    + bar::kHeadingGap
                    + bar::row_height() * static_cast<float>(p.points.size())
                    + line;                              // advice
    if (p.suggested_threshold_w > 0.0) h += line;
    if (!p.caveat.empty())             h += line * 2.0f; // wraps
    return h;
}

void draw_profile_chart(const profile::Profile& p, float width) {
    if (!p.valid) return;

    // --- Heading: the verdict, then what it rests on ------------------------
    char head[160];
    std::snprintf(head, sizeof head, "Rider profile   %s",
                  profile::phenotype_name(p.phenotype).c_str());
    theme::text_coloured(kTextPrimary, head);

    ImGui::SameLine();
    char basis[160];
    std::snprintf(basis, sizeof basis, "   %s power, against %.0f W threshold",
                  p.measured ? "measured" : "estimated", p.threshold_w);
    theme::text_coloured(kTextSecondary, basis);
    ImGui::Dummy(ImVec2(0.0f, bar::kHeadingGap));

    // --- Column geometry ----------------------------------------------------
    const float gap   = ImGui::GetStyle().ItemSpacing.x;
    const float avail = std::min(width, ImGui::GetContentRegionAvail().x);
    const float bar_w = std::max(120.0f, avail - kLabelW - kValueW - 3.0f * gap);
    const float half  = bar_w * 0.5f;

    ImDrawList*  draw   = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  row_h  = bar::row_height();
    const float  mid_x  = origin.x + kLabelW + gap + half;

    for (Size i = 0; i < p.points.size(); ++i) {
        const profile::Point& pt = p.points[i];

        const ImVec2 row_min(origin.x, origin.y + row_h * static_cast<float>(i));
        const float  text_y = row_min.y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
        const float  bar_y  = row_min.y + (row_h - bar::kBarHeight) * 0.5f;

        bar::text_at(ImVec2(row_min.x, text_y), kTextPrimary, label_for(pt.duration_s));

        if (!pt.found) {
            bar::text_at(ImVec2(mid_x - half, text_y), kTextMuted, "not reached");
            continue;
        }

        // The track spans the full scale either way, so every row is read
        // against the same ruler rather than against its own maximum.
        draw->AddRectFilled(ImVec2(mid_x - half, bar_y),
                            ImVec2(mid_x + half, bar_y + bar::kBarHeight),
                            bar::kTrack, bar::kBarRound);

        const Real  dev  = std::clamp(pt.shape - 1.0, -kFullScale, kFullScale);
        const float len  = static_cast<float>(std::abs(dev) / kFullScale) * half;
        const bool  up   = dev >= 0.0;
        const ImU32 col  = ImGui::GetColorU32(up ? kStrength : kWeakness);

        if (len > 1.0f) {
            const ImVec2 a(up ? mid_x : mid_x - len, bar_y);
            const ImVec2 b(up ? mid_x + len : mid_x, bar_y + bar::kBarHeight);
            draw->AddRectFilled(a, b, col, bar::kBarRound);
        }

        // Centre line last, so it reads on top of the bars that touch it.
        draw->AddLine(ImVec2(mid_x, bar_y - 2.0f),
                      ImVec2(mid_x, bar_y + bar::kBarHeight + 2.0f),
                      IM_COL32(255, 255, 255, 90), 1.0f);

        char value[128];
        std::snprintf(value, sizeof value, "%+.0f %%   %.0f W%s   %s",
                      (pt.shape - 1.0) * 100.0, pt.watts,
                      pt.wkg > 0.0 ? "" : "", system_for(pt.duration_s));
        bar::text_at(ImVec2(mid_x + half + gap, text_y),
                     up ? kStrength : kWeakness, value);
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(avail, row_h * static_cast<float>(p.points.size())));

    // --- What to do about it ------------------------------------------------
    const std::string advice = profile::phenotype_advice(p.phenotype);
    if (!advice.empty()) theme::text_coloured(kTextSecondary, advice);

    if (p.suggested_threshold_w > 0.0) {
        char note[220];
        std::snprintf(note, sizeof note,
                      "Threshold check: this ride's 20 min implies about %.0f W, "
                      "not %.0f W. The shape above is unaffected.",
                      p.suggested_threshold_w, p.threshold_w);
        theme::text_coloured(theme::kWarning, note);
    }
    if (!p.caveat.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail);
        theme::text_coloured(kTextMuted, "Note: " + p.caveat + ".");
        ImGui::PopTextWrapPos();
    }
}

} // namespace gui
