qt_add_library(gtm-sound STATIC
    song.cpp
)

target_link_libraries(gtm-sound PRIVATE
    gtm-util
    midifile
    Qt6::Core
)

target_include_directories(gtm-sound INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
