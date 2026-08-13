#pragma once

/// @file window.hpp
/// @brief The OS window, its OpenGL context and the ImGui/ImPlot lifetimes.
///
/// Four libraries have to be started in order and shut down in the reverse
/// order, and a miss is a crash at exit rather than a compile error. Tying all
/// of it to one object's lifetime means main() states what the program does
/// instead of how the toolkit is wired.

#include <functional>
#include <string>

namespace gui {

/// @brief An open window with a live ImGui frame loop.
class Window {
public:
    /// @brief Open the window and start ImGui, ImPlot and the render backends.
    ///
    /// Check ok() before use: failure is reported rather than thrown, so a
    /// machine with no display exits with a message instead of a stack trace.
    /// @param width  Initial width in pixels.
    /// @param height Initial height in pixels.
    /// @param title  Window title.
    Window(int width, int height, const char* title);

    /// @brief Shut everything down in the reverse order it was started.
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    /// @brief Whether construction succeeded.
    bool ok() const;

    /// @brief Whether the user has asked to close the window.
    bool should_close() const;

    /// @brief Call when files are dropped on the window.
    /// @param handler Receives one call per dropped path, in the order given.
    void on_file_dropped(std::function<void(const std::string&)> handler);

    /// @brief Pump events and open a new ImGui frame.
    ///
    /// Blocks while the window is minimised, so an idle GUI costs nothing.
    /// @return False when the frame should be skipped; do not draw or call
    ///         end_frame() in that case.
    bool begin_frame();

    /// @brief Render the frame just drawn and present it.
    void end_frame();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace gui
