#pragma once

/// @file app_window.hpp
/// @brief The window: the toolbar, and a tab per loaded file.

#include "file_view.hpp"

#include <string>
#include <vector>

namespace gui {

/// @brief The whole interface: analysis settings, and the open files.
///
/// The window owns nothing about any particular ride. It holds the settings
/// that describe the rider rather than the ride -- how many points to list,
/// whether to fetch wind -- and hands them to each FileView, so every file is
/// analysed on the same terms and a change re-runs all of them. Everything
/// else belongs to the file it came from.
class AppWindow {
public:
    /// @brief Draw one frame of the whole interface.
    void draw();

    /// @brief Open a GPX file in a tab of its own, and focus it.
    ///
    /// A file already open is focused rather than loaded twice. Safe to call
    /// with a path that does not exist: the error is shown in that file's
    /// banner instead of the report.
    ///
    /// @param path Path to the .gpx file. An empty string is ignored.
    void load(const std::string& path);

private:
    /// @brief The load button, the analysis settings, and Reload.
    void draw_toolbar();

    /// @brief One tab per open file; the active one draws its views.
    void draw_file_tabs();

    /// @brief Index of @p path among the open files, or -1.
    int index_of(const std::string& path) const;

    /// @brief Re-run every open file, after a setting changed.
    void reload_all();

    /// @brief The setting shared by every FileView: points listed first.
    std::size_t max_print() const;

    std::vector<FileView> files_;       ///< One per open file, in tab order.
    int                   active_ = 0;  ///< Which tab the toolbar acts on.
    std::string           start_dir_;   ///< Where the file dialog opens next.

    int max_print_ = 10;  ///< Track points to list (mirrors `--points`).

    /// Whether to apply historical wind, mirroring the command line's `--wind`.
    /// Off by default: it is the only control here that reaches the network, so
    /// it stays opt-in. Toggling it re-runs every open file, because the
    /// headwind term changes estimated power and everything derived from it.
    bool wind_on_ = false;
};

} // namespace gui
