# ---------------------------------------------------------------------------
# GPXAna GUI (Dear ImGui + ImPlot on GLFW/OpenGL3)
#
# This directory is entirely self-contained: it compiles the analysis code in
# ../src as a static library and adds the GUI on top. Nothing under ../src is
# modified, and this file is only reached when -DGPXANA_BUILD_GUI=ON, so the
# default command-line build never resolves a GUI dependency.
#
# Third-party sources are downloaded once into <repo>/external/ and reused
# afterwards, so they survive `rm -rf build`.
# ---------------------------------------------------------------------------

include(FetchContent)

# Keep the GUI's dependencies out of build/_deps (where the root project puts
# pugixml and nlohmann_json) and inside the source tree instead.
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/external)

# ---------------------------------------------------------------------------
# The analysis code, as a library
#
# Globbed rather than listed so modules added to ../src later are picked up
# without editing this file. CONFIGURE_DEPENDS makes CMake re-glob when the
# directory contents change. main.cpp is excluded because it defines main().
# ---------------------------------------------------------------------------

get_filename_component(GPXANA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src ABSOLUTE)

file(GLOB GPXANA_CORE_SRC CONFIGURE_DEPENDS ${GPXANA_SRC_DIR}/*.cpp)
list(FILTER GPXANA_CORE_SRC EXCLUDE REGEX "/main\\.cpp$")

if(NOT GPXANA_CORE_SRC)
    message(FATAL_ERROR "No analysis sources found in ${GPXANA_SRC_DIR}")
endif()

add_library(gpxana_core STATIC ${GPXANA_CORE_SRC})
target_include_directories(gpxana_core PUBLIC ${GPXANA_SRC_DIR})
target_link_libraries(gpxana_core PUBLIC pugixml nlohmann_json::nlohmann_json)
target_compile_features(gpxana_core PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# GLFW — use a system install when there is one, otherwise build it
# ---------------------------------------------------------------------------

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

find_package(glfw3 3.3 QUIET)
if(glfw3_FOUND)
    message(STATUS "GPXAna GUI: using system GLFW ${glfw3_VERSION}")
else()
    message(STATUS "GPXAna GUI: no system GLFW, building 3.5.1 in external/")
    # Wayland is off because its development headers are not assumed present;
    # X11 alone is enough. Docs/tests/examples would just slow the build down.
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.5.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui + ImPlot
#
# Neither ships a CMakeLists.txt, so FetchContent only downloads them and we
# compile the sources into one static library ourselves.
# ---------------------------------------------------------------------------

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui implot)

add_library(imgui_implot STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(imgui_implot PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)

# implot_items.cpp instantiates every plotting template over ten numeric types
# (ImS8 ... double). At -O3 that one translation unit takes minutes to compile
# and yields a ~7 MB object, twelve times the size of any other here. Every
# series in this GUI is double, so the list is cut to the two floating types.
# Restricting it is only safe while that stays true: plotting an int array would
# fail to link.
target_compile_definitions(imgui_implot
    PRIVATE "IMPLOT_CUSTOM_NUMERIC_TYPES=(float)(double)")
target_link_libraries(imgui_implot PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})
target_compile_features(imgui_implot PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# The GUI executable
# ---------------------------------------------------------------------------

add_executable(gpxana_gui
    src/main.cpp
    src/app_window.cpp
    src/analysis.cpp
    src/file_dialog.cpp
    src/window.cpp
    src/theme.cpp
    src/paths.cpp
    src/panel.cpp
    src/bar_row.cpp
    src/palette.cpp
    src/zone_chart.cpp
    src/hill_chart.cpp
    src/peaks_chart.cpp
    src/spectral_view.cpp
)
target_link_libraries(gpxana_gui PRIVATE gpxana_core imgui_implot)
target_compile_features(gpxana_gui PRIVATE cxx_std_17)

# The GUI sources compile clean under these, so turn them on to keep it that
# way: a truncating snprintf into an undersized buffer is exactly the kind of
# thing that only shows up once the value gets large. Applied to this target
# alone -- the analysis code and the vendored third-party sources are not this
# directory's to police.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gpxana_gui PRIVATE -Wall -Wextra)
endif()

# Put the binary in the top of the build directory, next to gpx_reader, rather
# than in the gui/ subdirectory this file lives in.
set_target_properties(gpxana_gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
