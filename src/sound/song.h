#pragma once
#ifndef SONG_H
#define SONG_H

#include <QList>
#include <memory>

#include "MidiFile.h"



class Song : public smf::MidiFile {
public:
    Song();
    Song(smf::MidiFile &midifile);

    Song(const Song&) = delete;
    Song &operator=(const Song&) = delete;

    ~Song();

    bool load();

    double durationInSeconds();
    int durationInTicks();

    std::vector<smf::MidiEventList *> tracks() { return this->m_events; }
    // std::vector<smf::MidiEvent *> events() {
    //     std::vector<smf::MidiEvent *> events;
    //     for (auto t : this->tracks()) {
    //         events.append_range(t->data());
    //     }
    //     return events;
    // }

    // trackList
    // notes()

    // Iterator for tracks in the song, basically wrapping std::vector iterator


private:
    // int m_track_count = 0;
    // smf::MidiFile m_midi_file;
};

#endif // SONG_H
