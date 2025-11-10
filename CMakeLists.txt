cmake_minimum_required(VERSION 3.16)

project(gtm VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(src)

include_directories(src)

qt_add_resources(gtm "gtm-media"
    FILES
        media/mus_title.mid
)
