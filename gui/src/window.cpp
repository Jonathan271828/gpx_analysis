#include "window.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>

#include <cstdio>

namespace gui {

namespace {

// OpenGL 3.0 is all ImGui's renderer backend needs.
constexpr int   kGlMajor    = 3;
constexpr int   kGlMinor    = 0;
constexpr char  kGlslVersion[] = "#version 130";
constexpr float kClear[4]   = {0.10f, 0.11f, 0.13f, 1.0f};

void report_glfw_error(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

} // namespace

struct Window::Impl {
    GLFWwindow*                             handle  = nullptr;
    std::function<void(const std::string&)> on_drop;

    bool glfw_started  = false;
    bool imgui_started = false;
    bool glfw_backend  = false;
    bool gl_backend    = false;

    // GLFW is a C API, so the drop callback finds its way back through the
    // window's user pointer rather than through a capture.
    static void forward_drop(GLFWwindow* w, int count, const char** paths) {
        if (count <= 0) return;
        auto* impl = static_cast<Impl*>(glfwGetWindowUserPointer(w));
        if (impl && impl->on_drop) impl->on_drop(paths[count - 1]);
    }
};

Window::Window(int width, int height, const char* title) {
    impl_ = new Impl;

    glfwSetErrorCallback(report_glfw_error);
    if (!glfwInit()) {
        std::fprintf(stderr, "Could not initialise GLFW.\n");
        return;
    }
    impl_->glfw_started = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, kGlMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, kGlMinor);

    impl_->handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!impl_->handle) {
        std::fprintf(stderr, "Could not create a window.\n");
        return;
    }
    glfwMakeContextCurrent(impl_->handle);
    glfwSwapInterval(1);   // vsync
    glfwSetWindowUserPointer(impl_->handle, impl_);
    glfwSetDropCallback(impl_->handle, &Impl::forward_drop);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    impl_->imgui_started = true;

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
    impl_->glfw_backend = ImGui_ImplGlfw_InitForOpenGL(impl_->handle, true);
    impl_->gl_backend   = ImGui_ImplOpenGL3_Init(kGlslVersion);
}

Window::~Window() {
    if (!impl_) return;
    if (impl_->gl_backend)    ImGui_ImplOpenGL3_Shutdown();
    if (impl_->glfw_backend)  ImGui_ImplGlfw_Shutdown();
    if (impl_->imgui_started) { ImPlot::DestroyContext(); ImGui::DestroyContext(); }
    if (impl_->handle)        glfwDestroyWindow(impl_->handle);
    if (impl_->glfw_started)  glfwTerminate();
    delete impl_;
}

bool Window::ok() const {
    return impl_ && impl_->handle && impl_->gl_backend;
}

bool Window::should_close() const {
    return !ok() || glfwWindowShouldClose(impl_->handle);
}

void Window::on_file_dropped(std::function<void(const std::string&)> handler) {
    impl_->on_drop = std::move(handler);
}

bool Window::begin_frame() {
    glfwPollEvents();

    // Nothing animates, so idle frames cost nothing.
    if (glfwGetWindowAttrib(impl_->handle, GLFW_ICONIFIED)) {
        glfwWaitEvents();
        return false;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    return true;
}

void Window::end_frame() {
    ImGui::Render();

    int width = 0, height = 0;
    glfwGetFramebufferSize(impl_->handle, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(kClear[0], kClear[1], kClear[2], kClear[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(impl_->handle);
}

} // namespace gui
