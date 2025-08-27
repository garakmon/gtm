qt_add_library(gtm-util STATIC
    util.cpp
)

target_include_directories(gtm-util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
