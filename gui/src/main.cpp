/// @file main.cpp
/// @brief Window, OpenGL context and frame loop for the GPXAna GUI.

#include "app_window.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Dropping a file onto the window loads the last one dropped.
void drop_callback(GLFWwindow* window, int count, const char** paths) {
    if (count <= 0) return;
    auto* app = static_cast<gui::AppWindow*>(glfwGetWindowUserPointer(window));
    if (app) app->load(paths[count - 1]);
}

} // namespace

int main(int argc, char* argv[]) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Could not initialise GLFW.\n");
        return EXIT_FAILURE;
    }

    // OpenGL 3.0 is all ImGui's renderer backend needs.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window =
        glfwCreateWindow(1280, 860, "GPXAna", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Could not create a window.\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // Plots live inside a long scrolling report, and by default ImPlot zooms on
    // the bare scroll wheel -- so scrolling the page would silently rescale
    // whichever plot the cursor passed over. Requiring Ctrl for zoom leaves the
    // plain wheel to scroll the page.
    ImPlot::GetInputMap().ZoomMod = ImGuiMod_Ctrl;

    // The interface is a single window sized to the viewport, so there is no
    // layout worth persisting: keep ImGui from dropping an imgui.ini in the
    // working directory.
    ImGui::GetIO().IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    gui::AppWindow app;
    glfwSetWindowUserPointer(window, &app);
    glfwSetDropCallback(window, drop_callback);

    // A path on the command line is loaded straight away.
    if (argc > 1) app.load(argv[1]);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Nothing animates, so idle frames cost nothing.
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.draw();

        ImGui::Render();
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
