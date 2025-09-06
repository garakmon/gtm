qt_add_library(gtm-app STATIC
    project.cpp
)

target_link_libraries(gtm-app PRIVATE
    gtm-util
    gtm-sound
    midifile
    Qt6::Core
)

target_include_directories(gtm-app INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
