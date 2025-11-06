#pragma once
#ifndef SONG_H
#define SONG_H

#include <QList>
#include <QMap>
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

    // reference so no copy wasting time
    QList<QPair<int, smf::MidiEvent *>> &getNotes() { return this->m_notes; }
    QMap<int, smf::MidiEvent *> &getTimeSignatures() { return this->m_time_signatures; }

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

    // Iterator for tracks in the song, basically wrapping std::vector iterator ?

    int getTickFromTime(double seconds);


private:
    // int m_track_count = 0;
    // smf::MidiFile m_midi_file;
    QMap<int, smf::MidiEvent *> m_time_signatures;
    QList<QPair<int, smf::MidiEvent *>> m_notes;
};

#endif // SONG_H
