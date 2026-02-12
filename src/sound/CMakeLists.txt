qt_add_library(gtm-sound STATIC
    song.cpp
    player.cpp
    mixer.cpp
    sequencer.cpp
)

if(APPLE)
    set(PLATFORM_LIBS
        "-framework CoreAudio" "-framework AudioToolbox"
        "-framework AudioUnit" "-framework CoreFoundation"
        "-framework CoreServices")
elseif(UNIX)
    set(PLATFORM_LIBS asound pthread)
elseif(WIN32)
    set(PLATFORM_LIBS winmm ole32 setupapi)
endif()

target_link_libraries(gtm-sound PUBLIC
    portaudio_static
    ${PLATFORM_LIBS}
    gtm-util
    midifile
    Qt6::Core
)

target_include_directories(gtm-sound INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/util
    ${CMAKE_SOURCE_DIR}/src/deps/portaudio/include
)
