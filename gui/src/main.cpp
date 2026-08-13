/// @file main.cpp
/// @brief Entry point: open a window, draw the interface until it is closed.

#include "app_window.hpp"
#include "window.hpp"

#include <cstdlib>

namespace {

constexpr int  kInitialWidth  = 1280;
constexpr int  kInitialHeight = 860;
constexpr char kTitle[]       = "GPXAna";

} // namespace

int main(int argc, char* argv[]) {
    gui::Window window(kInitialWidth, kInitialHeight, kTitle);
    if (!window.ok()) return EXIT_FAILURE;

    gui::AppWindow app;
    window.on_file_dropped([&app](const std::string& path) { app.load(path); });

    // A path on the command line is loaded straight away.
    if (argc > 1) app.load(argv[1]);

    while (!window.should_close()) {
        if (!window.begin_frame()) continue;
        app.draw();
        window.end_frame();
    }
    return EXIT_SUCCESS;
}
