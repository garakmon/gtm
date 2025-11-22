#pragma once
#ifndef SONG_H
#define SONG_H

#include <QList>
#include <QMap>
#include <QSet>
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
    QMap<int, smf::MidiEvent *> &getTempoChanges() { return this->m_tempo_changes; }
    QMap<int, smf::MidiEvent *> &getKeySignatures() { return this->m_key_signatures; }
    QList<smf::MidiEvent *> &getMarkers() { return this->m_markers; }
    QList<smf::MidiEvent *> &getMergedEvents() { return this->m_merged_events; }

    std::vector<smf::MidiEventList *> tracks() { return this->m_events; }
    bool isMetaTrack(int track) const { return m_meta_tracks.contains(track); }
    int getDisplayRow(int track) const { return m_meta_tracks.contains(0) ? track - 1 : track; }

    void setMetaInfo(const SongEntry &entry) { this->m_meta_info = entry; }
    const SongEntry& getMetaInfo() const { return this->m_meta_info; }

    int getTickFromTime(double seconds);

private:
    QMap<int, smf::MidiEvent *> m_time_signatures;
    QMap<int, smf::MidiEvent *> m_tempo_changes;
    QMap<int, smf::MidiEvent *> m_key_signatures;
    QList<smf::MidiEvent *> m_markers;
    QList<QPair<int, smf::MidiEvent *>> m_notes;
    QSet<int> m_meta_tracks;

    QList<smf::MidiEvent *> m_merged_events; // for playback

    SongEntry m_meta_info; 
};

#endif // SONG_H
