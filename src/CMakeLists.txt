qt_add_executable(gtm
    main.cpp
)

target_link_libraries(gtm PRIVATE
    gtm-ui
    gtm-util
    midifile
    Qt6::Widgets
)

set_target_properties(gtm PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

add_subdirectory(ui)
add_subdirectory(util)
add_subdirectory(deps)