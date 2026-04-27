#pragma once
#ifndef SONG_H
#define SONG_H

#include "deps/midifile/MidiFile.h"
#include "sound/soundtypes.h"
#include "util/constants.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QUndoStack>

#include <cstdint>
#include <vector>



//////////////////////////////////////////////////////////////////////////////////////////
///
/// The Song class is the gtm enriched MIDI model. It wraps a parsed smf::MidiFile, adding
/// some extra data the rest of the app needs for playback and editing.
/// Song does not perform playback itself, but it provides the structured timing and
/// event data that Sequencer, Controller, and the editor views depend on.
///
//////////////////////////////////////////////////////////////////////////////////////////
class Song : public smf::MidiFile {
public:
    struct DefaultEventSettings {
        double tempo_bpm = 120.0;
        int time_sig_num = 4;
        int time_sig_den = 4;
        int key_fifths = 0; // C major / A minor
        bool key_is_minor = false; // false = major, true = minor
        int channel = 0;
        int volume = 127;
        int pan = 64;
        int end_tick = 1;
    };

    Song() = default;
    Song(smf::MidiFile &midifile);
    ~Song() = default;

    Song(const Song &) = delete;
    Song &operator=(const Song &) = delete;
    Song(Song &&) = delete;
    Song &operator=(Song &&) = delete;

    // song loading
    bool load();
    void mergeEvents();
    void rebuildAfterTickEdit();
    void rebuildAfterDelete();
    static void addDefaultEvents(smf::MidiFile &midi, const VoiceGroup *voicegroup,
                                 const DefaultEventSettings &settings);

    // timing
    double durationInSeconds();
    int durationInTicks();
    int getTickFromTime(double seconds);

    // getters return references for efficiency
    QList<QPair<int, smf::MidiEvent *>> &getNotes() { return m_notes; }
    const QList<QPair<int, smf::MidiEvent *>> &getNotes() const { return m_notes; }
    QMap<int, smf::MidiEvent *> &getTimeSignatures() { return m_time_signatures; }
    const QMap<int, smf::MidiEvent *> &getTimeSignatures() const { return m_time_signatures; }
    QMap<int, smf::MidiEvent *> &getTempoChanges() { return m_tempo_changes; }
    const QMap<int, smf::MidiEvent *> &getTempoChanges() const { return m_tempo_changes; }
    QMap<int, smf::MidiEvent *> &getKeySignatures() { return m_key_signatures; }
    const QMap<int, smf::MidiEvent *> &getKeySignatures() const { return m_key_signatures; }
    QList<smf::MidiEvent *> &getMarkers() { return m_markers; }
    const QList<smf::MidiEvent *> &getMarkers() const { return m_markers; }
    QList<smf::MidiEvent *> &getMergedEvents() { return m_merged_events; }
    const QList<smf::MidiEvent *> &getMergedEvents() const { return m_merged_events; }

    // track access
    std::vector<smf::MidiEventList *> &tracks() { return m_events; }
    const std::vector<smf::MidiEventList *> &tracks() const { return m_events; }
    bool isMetaTrack(int track) const { return m_meta_tracks.contains(track); }
    bool isTrackEmpty(int track) const;
    int getDisplayRow(int track) const { return m_meta_tracks.contains(0) ? track - 1 : track; }
    void appendEmptyTrack();
    bool deleteTrackAt(int track);
    bool deleteTrackAndStore(int track, smf::MidiEventList &track_data);
    bool restoreTrack(int track, smf::MidiEventList &track_data);

    // song metadata
    void setMetaInfo(const SongEntry &entry) { m_meta_info = entry; }
    const SongEntry &getMetaInfo() const { return m_meta_info; }

    // initial channel state info
    const uint8_t *getInitialPrograms() const { return m_initial_programs; }
    struct InitialChannelState {
        uint8_t volume = 100;
        uint8_t pan = 0x40;
        uint8_t expression = 0x7f;
        int16_t pitch_bend = 0;
    };
    const InitialChannelState *getInitialChannelStates() const { return m_initial_channel_states; }

    // edit status
    bool isClean() const { return m_clean; }
    void setClean(bool clean = true) { m_clean = clean; }
    void markDirty() { m_clean = false; }
    QUndoStack *editHistory() { return &m_edit_history; }
    const QUndoStack *editHistory() const { return &m_edit_history; }

private:
    static int defaultProgramFromVoicegroup(const VoiceGroup *voicegroup);
    void extractInitialState();
    void rebuildNotesIndex();
    void reindexTrackEvents();

private:
    // event data
    QMap<int, smf::MidiEvent *> m_time_signatures;
    QMap<int, smf::MidiEvent *> m_tempo_changes;
    QMap<int, smf::MidiEvent *> m_key_signatures;
    QList<smf::MidiEvent *> m_markers;
    QList<QPair<int, smf::MidiEvent *>> m_notes;
    QList<smf::MidiEvent *> m_merged_events;

    // song meta info
    QSet<int> m_meta_tracks;
    SongEntry m_meta_info;

    // initial channel state
    uint8_t m_initial_programs[g_num_midi_channels] = {0};
    InitialChannelState m_initial_channel_states[g_num_midi_channels];

    // edit state
    bool m_clean = true;
    QUndoStack m_edit_history;
};

#endif // SONG_H
