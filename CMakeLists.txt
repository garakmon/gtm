cmake_minimum_required(VERSION 3.16)

project(gtm VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GTM_TIME_COMPILES "Print per-file compile times in debug builds" OFF)

if(GTM_TIME_COMPILES AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(GTM_COMPILE_TIMER
        "${Python3_EXECUTABLE};${CMAKE_SOURCE_DIR}/tools/compile_timer.py")
    set(CMAKE_C_COMPILER_LAUNCHER "${GTM_COMPILE_TIMER}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${GTM_COMPILE_TIMER}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()

add_subdirectory(src)

include_directories(src)

qt_add_resources(gtm "gtm-media"
    FILES
        media/mus_title.mid
)
