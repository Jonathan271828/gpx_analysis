#pragma once

/// @file file_dialog.hpp
/// @brief A native "open file" dialog, via the zenity command-line tool.

#include <string>

namespace gui {

/// @brief Ask the user for a .gpx file with a native file chooser.
/// @param start_dir Directory to open in; updated to the chosen file's folder
///                  so the next call starts where this one left off.
/// @return The selected path, or an empty string if the user cancelled or no
///         dialog program is available.
std::string open_gpx_file(std::string& start_dir);

/// @brief True when a file-chooser program was found on this system.
bool file_dialog_available();

} // namespace gui
