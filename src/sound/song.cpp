#include "song.h"



Song::Song() {
    //
}

Song::Song(smf::MidiFile &midifile) : smf::MidiFile(midifile) {
    // move to load()?
    //m_track_count = m_midi_file.getTrackCount();
    //this->linkNotePairs();
}

Song::~Song() {
    //
}

bool Song::load() {
    //
}
