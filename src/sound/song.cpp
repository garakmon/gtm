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

double Song::durationInSeconds() {
    this->doTimeAnalysis();
    this->joinTracks();
    double end_time = (*this)[0].last().seconds;
    this->splitTracks();
    return end_time;
}

int Song::durationInTicks() {
    this->joinTracks();
    int end_tick = (*this)[0].last().tick;
    this->splitTracks();
    return end_tick;
}
