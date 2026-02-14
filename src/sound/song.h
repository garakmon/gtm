#pragma once
#ifndef SONG_H
#define SONG_H

#include <QList>
#include <QMap>
#include <QSet>
#include <memory>

#include "sound/soundtypes.h"
#include "deps/midifile/MidiFile.h"



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
    const QList<QPair<int, smf::MidiEvent *>> &getNotes() const { return this->m_notes; }
    QMap<int, smf::MidiEvent *> &getTimeSignatures() { return this->m_time_signatures; }
    const QMap<int, smf::MidiEvent *> &getTimeSignatures() const { return this->m_time_signatures; }
    QMap<int, smf::MidiEvent *> &getTempoChanges() { return this->m_tempo_changes; }
    const QMap<int, smf::MidiEvent *> &getTempoChanges() const { return this->m_tempo_changes; }
    QMap<int, smf::MidiEvent *> &getKeySignatures() { return this->m_key_signatures; }
    const QMap<int, smf::MidiEvent *> &getKeySignatures() const { return this->m_key_signatures; }
    QList<smf::MidiEvent *> &getMarkers() { return this->m_markers; }
    const QList<smf::MidiEvent *> &getMarkers() const { return this->m_markers; }
    QList<smf::MidiEvent *> &getMergedEvents() { return this->m_merged_events; }
    const QList<smf::MidiEvent *> &getMergedEvents() const { return this->m_merged_events; }

    std::vector<smf::MidiEventList *> tracks() { return this->m_events; }
    std::vector<smf::MidiEventList *> tracks() const { return this->m_events; }
    bool isMetaTrack(int track) const { return m_meta_tracks.contains(track); }
    int getDisplayRow(int track) const { return m_meta_tracks.contains(0) ? track - 1 : track; }

    void setMetaInfo(const SongEntry &entry) { this->m_meta_info = entry; }
    const SongEntry& getMetaInfo() const { return this->m_meta_info; }

    int getTickFromTime(double seconds);

    // Initial state per channel, extracted from track headers before merging
    const uint8_t *getInitialPrograms() const { return m_initial_programs; }

    struct InitialChannelState {
        uint8_t volume = 100;      // CC7
        uint8_t pan = 64;          // CC10 (center)
        uint8_t expression = 127;  // CC11
        int16_t pitch_bend = 0;    // center = 0 (raw value 8192)
    };
    const InitialChannelState *getInitialChannelStates() const { return m_initial_channel_states; }
    bool isClean() const { return m_clean; }
    void setClean(bool clean = true) { m_clean = clean; }
    void markDirty() { m_clean = false; }

private:
    void extractInitialState();
    QMap<int, smf::MidiEvent *> m_time_signatures;
    QMap<int, smf::MidiEvent *> m_tempo_changes;
    QMap<int, smf::MidiEvent *> m_key_signatures;
    QList<smf::MidiEvent *> m_markers;
    QList<QPair<int, smf::MidiEvent *>> m_notes;
    QSet<int> m_meta_tracks;

    QList<smf::MidiEvent *> m_merged_events; // for playback

    SongEntry m_meta_info;

    // Initial state per channel (extracted from track headers)
    uint8_t m_initial_programs[16] = {0};
    InitialChannelState m_initial_channel_states[16];
    bool m_clean = true;
};

#endif // SONG_H
