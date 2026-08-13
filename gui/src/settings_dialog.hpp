#pragma once

/// @file settings_dialog.hpp
/// @brief Editor for the persistent defaults in the config file.

#include <map>
#include <string>

namespace gui {

/// @brief The settings editor's state, held across frames by the window.
///
/// The values are kept as text keyed by the config field name, which is also
/// the command-line flag. Nothing here knows what any particular setting means:
/// the dialog is built by walking config::fields(), so a setting added to the
/// library appears here without a change on this side.
struct SettingsEditor {
    std::map<std::string, std::string> values;  ///< Field key to value as text.
    std::string path;                           ///< File being edited.
    std::string status;                         ///< Last save result, if any.
    bool        open = false;                   ///< Whether to show the modal.
};

/// @brief Fill @p editor from the config file, falling back to the built-in
/// defaults for anything the file does not set, and mark it for display.
///
/// Showing effective values rather than blanks matters: a rider needs to see
/// what the tool is currently doing before deciding what to change.
///
/// @param editor The editor to populate.
void open_settings(SettingsEditor& editor);

/// @brief Draw the modal if it is open.
///
/// @param editor The editor state; @ref SettingsEditor::open is cleared when
///               the dialog closes.
/// @return True when the user saved, so the caller can re-run its analyses --
///         every open ride's numbers depend on these.
bool draw_settings_dialog(SettingsEditor& editor);

} // namespace gui
