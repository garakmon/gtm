qt_add_library(gtm-util STATIC
    util.cpp
    gtmconfig.cpp
    theme.cpp
)

target_link_libraries(gtm-util PUBLIC
    orderedjson
    Qt6::Widgets
    Qt6::Core
)

target_include_directories(gtm-util PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
