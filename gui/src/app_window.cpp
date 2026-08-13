#include "app_window.hpp"

#include "compare_view.hpp"
#include "file_dialog.hpp"
#include "palette.hpp"
#include "settings_dialog.hpp"
#include "theme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <vector>
#include <string>

namespace gui {

void AppWindow::load(const std::string& path) {
    if (path.empty()) return;

    // Opening the same ride twice would give two tabs with one name and no way
    // to tell them apart, so an open file is focused instead.
    const int existing = index_of(path);
    if (existing >= 0) {
        active_ = existing;
        return;
    }

    files_.emplace_back(path, max_print(), wind_on_);
    active_ = static_cast<int>(files_.size()) - 1;
}

int AppWindow::index_of(const std::string& path) const {
    for (std::size_t i = 0; i < files_.size(); ++i)
        if (files_[i].path() == path) return static_cast<int>(i);
    return -1;
}

std::size_t AppWindow::max_print() const {
    return static_cast<std::size_t>(max_print_ < 0 ? 0 : max_print_);
}

void AppWindow::reload_all() {
    for (FileView& f : files_) f.reload(max_print(), wind_on_);
}

void AppWindow::draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("GPXAna", nullptr, flags)) {
        draw_toolbar();
        ImGui::Separator();
        draw_file_tabs();

        // Saved settings change every open ride's numbers, so all are re-run.
        if (draw_settings_dialog(settings_)) reload_all();
    }
    ImGui::End();
}

void AppWindow::draw_file_tabs() {
    if (files_.empty()) {
        ImGui::TextDisabled(
            "Load one or more GPX activities to see their analysis here.");
        return;
    }

    if (active_ >= static_cast<int>(files_.size())) active_ = 0;

    // Reorderable so several rides can be arranged for comparison, and closable
    // so a long session does not accumulate tabs.
    const ImGuiTabBarFlags bar_flags = ImGuiTabBarFlags_Reorderable |
                                       ImGuiTabBarFlags_AutoSelectNewTabs |
                                       ImGuiTabBarFlags_FittingPolicyScroll;

    int closed = -1;
    if (ImGui::BeginTabBar("files", bar_flags)) {
        for (std::size_t i = 0; i < files_.size(); ++i) {
            bool open = true;
            const bool shown = ImGui::BeginTabItem(files_[i].title().c_str(), &open);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", files_[i].path().c_str());

            if (shown) {
                active_ = static_cast<int>(i);

                // Every view inside uses fixed names for its tab bar, child
                // regions and plots, and ImPlot files a plot's axis range under
                // its id -- so without an id unique to this file, two rides
                // would share one zoom and one channel selection.
                ImGui::PushID(static_cast<int>(i));
                files_[i].draw();
                ImGui::PopID();

                ImGui::EndTabItem();
            }
            if (!open) closed = static_cast<int>(i);
        }

        // Trailing so it stays at the right-hand end however the file tabs are
        // reordered; only offered once there is more than one ride to compare.
        if (files_.size() > 1 &&
            ImGui::BeginTabItem("Compare", nullptr, ImGuiTabItemFlags_Trailing)) {
            draw_compare_tab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Erased after the bar is finished: removing an entry mid-iteration would
    // leave ImGui drawing a tab that no longer exists.
    if (closed >= 0) {
        files_.erase(files_.begin() + closed);
        if (active_ > closed) --active_;
    }
}

// Opens the editor for the persistent defaults. Offered whether or not a file
// is loaded, since the settings describe the rider rather than any one ride.
void AppWindow::draw_settings_button() {
    ImGui::SameLine();
    if (ImGui::Button("Settings...")) open_settings(settings_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Threshold power, weight, aerodynamics and the rest,\n"
                          "kept in a config file the command line reads too.");
}

void AppWindow::draw_compare_tab() {
    std::vector<CompareRide> rides;
    rides.reserve(files_.size());
    for (const FileView& f : files_)
        rides.push_back({f.title(), &f.channels(), &f.distance()});

    const std::vector<std::string> names = compare_channels(rides);
    if (names.empty()) {
        ImGui::TextDisabled("The open files carry no channels to compare.");
        return;
    }
    if (compare_on_.size() != names.size()) compare_on_.assign(names.size(), 1);

    const ImGuiStyle& style  = ImGui::GetStyle();
    const float       page_w = ImGui::GetContentRegionAvail().x - style.ScrollbarSize;

    ImGui::TextUnformatted("Line up by:");
    ImGui::SameLine();
    int  axis         = static_cast<int>(compare_axis_);
    bool axis_changed = ImGui::RadioButton("distance", &axis,
                                           static_cast<int>(CompareAxis::Distance));
    ImGui::SameLine();
    axis_changed |= ImGui::RadioButton("elapsed time", &axis,
                                       static_cast<int>(CompareAxis::Elapsed));
    if (axis_changed) {
        compare_axis_  = static_cast<CompareAxis>(axis);
        compare_range_ = Span{};   // the numbers mean something else now
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset zoom")) compare_range_ = Span{};

    ImGui::TextUnformatted("Channels:");
    for (std::size_t i = 0; i < names.size(); ++i) {
        ImGui::SameLine();
        bool on = compare_on_[i] != 0;
        if (ImGui::Checkbox((names[i] + "##cmp").c_str(), &on))
            compare_on_[i] = on ? 1 : 0;
    }

    // Colour means the ride here, not the channel, so the key has to be present
    // rather than inferred from the plot legends one at a time.
    ImGui::TextUnformatted("Rides:");
    for (std::size_t i = 0; i < files_.size(); ++i) {
        ImGui::SameLine();
        theme::text_coloured(series_colour(i), files_[i].title());
    }

    ImGui::Separator();

    ImGui::BeginChild("compare", ImVec2(0.0f, 0.0f));
    draw_compare_plots(rides, names, compare_on_, compare_axis_, compare_range_,
                       page_w - 12.0f, 190.0f);
    ImGui::EndChild();
}

void AppWindow::draw_toolbar() {
    if (file_dialog_available()) {
        if (ImGui::Button("Load GPX...")) {
            for (const std::string& chosen : open_gpx_files(start_dir_))
                load(chosen);
        }
    } else {
        // No zenity on this system: let the path be typed instead.
        static char typed[1024] = "";
        ImGui::SetNextItemWidth(360.0f);
        const bool entered = ImGui::InputTextWithHint(
            "##path", "path to a .gpx file, then press Enter", typed,
            IM_ARRAYSIZE(typed), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Load") || entered) && typed[0] != '\0')
            load(typed);
    }

    ImGui::SameLine();
    if (files_.empty()) {
        ImGui::TextDisabled("no files loaded - drag .gpx files here, or use the button");
        return;
    }

    if (ImGui::Button("Reload all")) reload_all();

    // Mirrors the CLI's --points: how many track points each report lists first.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("track points", &max_print_)) {
        if (max_print_ < 0) max_print_ = 0;
        reload_all();
    }

    // Mirrors the command line's --wind. Off by default because it is the only
    // control that reaches the network; ticking it re-runs every open file,
    // since the headwind term changes estimated power and all that follows.
    ImGui::SameLine();
    if (ImGui::Checkbox("wind", &wind_on_)) reload_all();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fetch historical wind from Open-Meteo and apply it to\n"
                          "the aero term, as the command line's --wind does.\n"
                          "Leaves the network alone while unticked.");

    ImGui::SameLine();
    ImGui::TextDisabled("(%zu file%s open)", files_.size(),
                        files_.size() == 1 ? "" : "s");

    draw_settings_button();
}

} // namespace gui
