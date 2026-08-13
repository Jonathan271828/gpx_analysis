#include "settings_dialog.hpp"

#include "config.hpp"
#include "theme.hpp"

#include "imgui.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace gui {

namespace {

constexpr const char* kTitle       = "Settings";
constexpr float       kFieldWidth  = 150.0f;
constexpr float       kLabelColumn = 230.0f;

/// Trailing zeros make a settings form look like a spreadsheet; "0.32" reads
/// as a choice, "0.320000" as a readout.
std::string tidy(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

void draw_field(const config::Field& f, std::map<std::string, std::string>& values) {
    std::string& text = values[f.key];

    ImGui::TextUnformatted(f.label.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("--%s\n%s", f.key.c_str(), f.help.c_str());
    ImGui::SameLine(kLabelColumn);

    ImGui::PushID(f.key.c_str());
    ImGui::SetNextItemWidth(kFieldWidth);

    if (f.kind == config::Kind::Flag) {
        bool on = (text == "true");
        if (ImGui::Checkbox("##v", &on)) text = on ? "true" : "false";
    } else if (f.kind == config::Kind::Int) {
        int v = std::atoi(text.c_str());
        if (ImGui::InputInt("##v", &v)) {
            if (v < 0) v = 0;
            text = std::to_string(v);
        }
    } else {
        double v = std::atof(text.c_str());
        if (ImGui::InputDouble("##v", &v, 0.0, 0.0, "%.6g")) text = tidy(v);
    }
    ImGui::PopID();

    if (!f.unit.empty()) {
        ImGui::SameLine();
        theme::text_coloured(theme::kTextSecondary, f.unit);
    }
}

} // namespace

void open_settings(SettingsEditor& editor) {
    editor.path   = config::default_path();
    editor.status.clear();

    editor.values = config::builtin_defaults();

    std::string err;
    for (const auto& [key, value] : config::load_values(editor.path, err))
        editor.values[key] = value;
    if (!err.empty()) editor.status = err;

    editor.open = true;
}

bool draw_settings_dialog(SettingsEditor& editor) {
    if (editor.open && !ImGui::IsPopupOpen(kTitle)) ImGui::OpenPopup(kTitle);
    if (!editor.open) return false;

    bool saved = false;
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        theme::text_coloured(theme::kTextSecondary,
                             "Defaults for every ride, used by the command line too.");
        theme::text_coloured(theme::kTextMuted,
                             editor.path.empty() ? "(no writable config location)"
                                                 : editor.path);
        ImGui::Separator();

        // Grouped by walking the field list, so the dialog and the file share
        // one order and a new setting needs no change here.
        std::string group;
        for (const config::Field& f : config::fields()) {
            if (f.group != group) {
                group = f.group;
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                theme::text_coloured(theme::kTextPrimary, group);
                ImGui::Separator();
            }
            draw_field(f, editor.values);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(110.0f, 0.0f))) {
            std::string err;
            if (config::save(editor.path, editor.values, err)) {
                saved       = true;
                editor.open = false;
                ImGui::CloseCurrentPopup();
            } else {
                editor.status = err;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
            editor.open = false;
            ImGui::CloseCurrentPopup();
        }

        // Restores what the tool does with no config file, rather than some
        // second set of numbers kept here; the file is only rewritten on Save.
        ImGui::SameLine();
        if (ImGui::Button("Reset to defaults", ImVec2(150.0f, 0.0f)))
            editor.values = config::builtin_defaults();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Load the built-in values. Nothing is written\n"
                              "until you press Save.");

        if (!editor.status.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            theme::text_coloured(theme::kWarning, editor.status);
        }
        ImGui::EndPopup();
    }
    return saved;
}

} // namespace gui
