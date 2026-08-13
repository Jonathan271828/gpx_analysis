cmake_minimum_required(VERSION 3.16)
project(GPXAna CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# HTTP for the Open-Meteo wind API is done by invoking the `curl` command-line
# tool at runtime (see src/wind.cpp), so no libcurl dev package is required.

# Fetch pugixml (XML) and nlohmann/json (weather API responses)
include(FetchContent)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(pugixml nlohmann_json)

add_executable(gpx_reader
    src/main.cpp
    src/app.cpp
    src/arg_parser.cpp
    src/config.cpp
    src/io_base.cpp
    src/screen_output.cpp
    src/file_output.cpp
    src/channels.cpp
    src/gpx_reader.cpp
    src/wind.cpp
    src/signal.cpp
    src/metrics.cpp
    src/zones.cpp
    src/splits.cpp
    src/cp_model.cpp
    src/trends.cpp
    src/ride.cpp
    src/peaks.cpp
    src/profile.cpp
    src/durability.cpp
    src/quadrant.cpp
)

target_link_libraries(gpx_reader PRIVATE
    pugixml
    nlohmann_json::nlohmann_json
)
target_include_directories(gpx_reader PRIVATE src)

# Optional GUI (Dear ImGui + ImPlot). Off by default, so the command-line build
# above is untouched: nothing extra is downloaded and no GUI dependency is
# resolved. Everything the GUI needs lives in gui/. Enable with:
#   cmake -S . -B build -DGPXANA_BUILD_GUI=ON
option(GPXANA_BUILD_GUI "Build the ImGui/ImPlot GUI (gpxana_gui)" OFF)
if(GPXANA_BUILD_GUI)
    add_subdirectory(gui)
endif()
