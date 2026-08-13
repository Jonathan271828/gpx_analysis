#include "palette.hpp"

#include <cstdio>
#include <string>

namespace gui {

namespace {

// ---------------------------------------------------------------------------
// Zone palette
//
// The documented categorical palette's first seven slots, dark column, in their
// fixed order. That order is the colourblind-safety mechanism, so it is not
// rearranged: validated for adjacent pairs against this surface at worst CVD
// dE 8.4 and worst normal-vision dE 19.3, all seven above 3:1 contrast.
//
// Zones are an ordered scale, so a single-hue ramp or a cool-to-hot "semantic
// heat" ramp would be the natural choices. Both were measured and rejected:
//
//   * one-hue ramp: the documented blue steps are ~0.047 apart in lightness,
//     and between the dark-surface floor (step 600) and the lightest step there
//     is room for only six at the required 0.06 spacing -- one short of seven.
//   * cool-to-hot ramp: adjacent warm hues collapse. yellow<->orange is dE 4.8
//     under deuteranopia and red<->orange only 7.1 even with normal vision,
//     against floors of 8 and 15.
//
// Order is carried by position and labels instead, and everything coloured this
// way is either directly labelled or accompanied by a key.
// ---------------------------------------------------------------------------

const ImVec4 kZone[7] = {
    ImVec4(0.224f, 0.529f, 0.898f, 1.0f),   // slot 1  blue     #3987e5
    ImVec4(0.851f, 0.349f, 0.149f, 1.0f),   // slot 2  orange   #d95926
    ImVec4(0.098f, 0.620f, 0.439f, 1.0f),   // slot 3  aqua     #199e70
    ImVec4(0.788f, 0.522f, 0.000f, 1.0f),   // slot 4  yellow   #c98500
    ImVec4(0.835f, 0.318f, 0.506f, 1.0f),   // slot 5  magenta  #d55181
    ImVec4(0.000f, 0.514f, 0.000f, 1.0f),   // slot 6  green    #008300
    ImVec4(0.565f, 0.522f, 0.914f, 1.0f),   // slot 7  violet   #9085e9
};

// The zone's short tag: "Z3 Tempo" -> "Z3".
std::string short_tag(const std::string& label) {
    const std::string::size_type sp = label.find(' ');
    return (sp == std::string::npos) ? label : label.substr(0, sp);
}

} // namespace

ImVec4 zone_colour_vec(std::size_t index) {
    return kZone[index % (sizeof kZone / sizeof kZone[0])];
}

ImU32 zone_colour(std::size_t index) {
    return ImGui::GetColorU32(zone_colour_vec(index));
}

ImVec4 series_colour(std::size_t index) {
    return zone_colour_vec(index);
}

void zone_swatch(std::size_t index) {
    const float  sw = ImGui::GetTextLineHeight() * 0.42f;
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  y  = p.y + (ImGui::GetTextLineHeight() - sw) * 0.5f;
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p.x, y), ImVec2(p.x + sw, y + sw), zone_colour(index), 2.0f);
    ImGui::Dummy(ImVec2(sw, ImGui::GetTextLineHeight()));
}

std::string zone_bounds_text(const zones::Zone& zone, const std::string& unit) {
    char buf[64];
    if (zone.hi < 0.0)
        std::snprintf(buf, sizeof buf, "%.0f+ %s", zone.lo, unit.c_str());
    else
        std::snprintf(buf, sizeof buf, "%.0f-%.0f %s", zone.lo, zone.hi, unit.c_str());
    return buf;
}

std::string zone_label(const zones::ZoneTable& table, int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= table.zones.size()) return {};
    return table.zones[static_cast<std::size_t>(index)].label;
}

void draw_zone_legend(const zones::ZoneTable& table) {
    if (!table.valid || table.zones.empty()) return;

    for (std::size_t i = 0; i < table.zones.size(); ++i) {
        const zones::Zone& z = table.zones[i];

        ImGui::SameLine();
        zone_swatch(i);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextUnformatted(short_tag(z.label).c_str());

        // The full name and bounds stay one hover away rather than crowding the
        // line, which has to hold seven entries.
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", z.label.c_str(),
                              zone_bounds_text(z, "W").c_str());
    }
}

} // namespace gui
