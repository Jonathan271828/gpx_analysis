cmake_minimum_required(VERSION 3.16)
project(gpx_reader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch pugixml
include(FetchContent)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
)
FetchContent_MakeAvailable(pugixml)

add_executable(gpx_reader
    src/main.cpp
    src/gpx_reader.cpp
)

target_link_libraries(gpx_reader PRIVATE pugixml)
target_include_directories(gpx_reader PRIVATE src)
