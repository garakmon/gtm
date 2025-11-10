qt_add_library(gtm-app STATIC
    project.cpp
    controller.cpp
    projectinterface.cpp
)

target_link_libraries(gtm-app PUBLIC
    gtm-ui
    midifile
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(gtm-app INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/deps/portaudio/include
    ${CMAKE_SOURCE_DIR}/src/sound
)
