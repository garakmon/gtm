qt_add_library(gtm-app STATIC
    project.cpp
    controller.cpp
    projectinterface.cpp
)

set_target_properties(gtm-app PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/src/ui"
)

target_link_libraries(gtm-app PUBLIC
    gtm-ui
    midifile
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(gtm-app PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/deps/portaudio/include
    ${CMAKE_SOURCE_DIR}/src/sound
)
