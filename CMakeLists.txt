cmake_minimum_required(VERSION 3.16)
project(gpx_reader CXX)

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
    src/gpx_reader.cpp
    src/wind.cpp
)

target_link_libraries(gpx_reader PRIVATE
    pugixml
    nlohmann_json::nlohmann_json
)
target_include_directories(gpx_reader PRIVATE src)
