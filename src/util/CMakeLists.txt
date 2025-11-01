qt_add_library(gtm-util STATIC
    util.cpp
)

target_link_libraries(gtm-util PRIVATE
    Qt6::Core
)

target_include_directories(gtm-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
