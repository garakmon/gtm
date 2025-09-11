qt_add_library(gtm-app STATIC
    project.cpp
    controller.cpp
)

target_link_libraries(gtm-app PRIVATE
    gtm-ui
    gtm-util
    gtm-sound
    midifile
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(gtm-app INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
