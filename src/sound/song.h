#pragma once
#ifndef SONG_H
#define SONG_H

#include <QList>
#include <QMap>
#include <memory>

#include "MidiFile.h"
#include "soundtypes.h"



class Song : public smf::MidiFile {
public:
    Song();
    Song(smf::MidiFile &midifile);

    Song(const Song&) = delete;
    Song &operator=(const Song&) = delete;

    ~Song();

    bool load();
    void mergeEvents();

    double durationInSeconds();
    int durationInTicks();

    // reference so no copy wasting time
    QList<QPair<int, smf::MidiEvent *>> &getNotes() { return this->m_notes; }
    QMap<int, smf::MidiEvent *> &getTimeSignatures() { return this->m_time_signatures; }
    QList<smf::MidiEvent *> &getMergedEvents() { return this->m_merged_events; }

    std::vector<smf::MidiEventList *> tracks() { return this->m_events; }

    void setMetaInfo(const SongEntry &entry) { this->m_meta_info = entry; }

    int getTickFromTime(double seconds);

private:
    QMap<int, smf::MidiEvent *> m_time_signatures;
    QList<QPair<int, smf::MidiEvent *>> m_notes;

    QList<smf::MidiEvent *> m_merged_events; // for playback

    SongEntry m_meta_info; // 
};

#endif // SONG_H
