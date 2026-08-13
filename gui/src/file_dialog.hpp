#pragma once

/// @file file_dialog.hpp
/// @brief A native "open file" dialog, via the zenity command-line tool.

#include <string>
#include <vector>

namespace gui {

/// @brief Ask the user for one or more .gpx files with a native file chooser.
///
/// Several may be chosen at once, since each opens in a tab of its own and a
/// session usually means comparing rides rather than looking at one.
///
/// @param start_dir Directory to open in; updated to the chosen files' folder
///                  so the next call starts where this one left off.
/// @return The selected paths, in the order the chooser reported them; empty
///         if the user cancelled or no dialog program is available.
std::vector<std::string> open_gpx_files(std::string& start_dir);

/// @brief True when a file-chooser program was found on this system.
bool file_dialog_available();

} // namespace gui
