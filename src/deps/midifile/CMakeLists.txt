qt_add_library(midifile STATIC
    Binasc.cpp
    MidiEvent.cpp
    MidiEventList.cpp
    MidiFile.cpp
    MidiMessage.cpp
)

target_include_directories(midifile INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

