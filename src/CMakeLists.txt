qt_add_executable(gtm
    main.cpp
)

target_link_libraries(gtm PRIVATE
    gtm-ui
    gtm-app
    gtm-sound
    midifile
    Qt6::Widgets
    Qt6::Core
)

set_target_properties(gtm PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

add_subdirectory(deps)
add_subdirectory(util)
add_subdirectory(ui)
add_subdirectory(app)
add_subdirectory(sound)
