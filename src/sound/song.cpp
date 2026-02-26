#include "sound/song.h"

#include <algorithm>
#include <cmath>


namespace {
constexpr int k_pitch_bend_center = 8192;
} // namespace



/**
 * Create this song from a parsed midi file.
 */
Song::Song(smf::MidiFile &midifile) : smf::MidiFile(midifile) { }

/**
 * Load song data from the MidiFile.
 * Importantly, call linkNotePairs() and doTimeAnalysis().
 */
bool Song::load() {
    this->doTimeAnalysis();
    this->linkNotePairs();

    m_meta_tracks.clear();
    m_time_signatures.clear();
    m_tempo_changes.clear();
    m_key_signatures.clear();
    m_markers.clear();
    m_notes.clear();

    this->extractInitialState();

    int track_num = 0;
    for (auto *track : this->tracks()) {
        bool has_notes = false;

        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *midi_event = &(*track)[i];

            if (midi_event->isTimeSignature()) {
                m_time_signatures[midi_event->tick] = midi_event;
            }
            else if (midi_event->isTempo()) {
                m_tempo_changes[midi_event->tick] = midi_event;
            }
            else if (midi_event->isKeySignature()) {
                m_key_signatures[midi_event->tick] = midi_event;
            }
            else if (midi_event->isMarkerText() || midi_event->isText()) {
                m_markers.append(midi_event);
            }
            else if (midi_event->isNoteOn()) {
                m_notes.append({track_num, midi_event});
                has_notes = true;
            }
        }

        if (!has_notes) {
            m_meta_tracks.insert(track_num);
        }

        track_num++;
    }

    this->mergeEvents();
    return true;
}

/**
 * Merge all events into one tick-ordered list.
 * This makes playback much easier.
 */
void Song::mergeEvents() {
    m_merged_events.clear();

    int track_count = this->getTrackCount();
    int total_events = 0;
    for (int t = 0; t < track_count; t++) {
        total_events += this->getEventCount(t);
    }

    m_merged_events.reserve(total_events);

    for (int track_index = 0; track_index < track_count; track_index++) {
        smf::MidiEventList &event_list = *m_events[track_index];
        int event_count = event_list.size();

        for (int i = 0; i < event_count; ++i) {
            m_merged_events.append(&event_list[i]);
        }
    }

    // preserve event order for identical ticks
    std::stable_sort(m_merged_events.begin(), m_merged_events.end(),
        [](smf::MidiEvent *a, smf::MidiEvent *b) {
            return a->tick < b->tick;
        }
    );
}

double Song::durationInSeconds() {
    this->doTimeAnalysis();
    this->joinTracks();

    if (this->getTrackCount() <= 0 || (*this)[0].size() == 0) {
        this->splitTracks();
        return 0.0;
    }

    double end_time = (*this)[0].last().seconds;
    this->splitTracks();
    return end_time;
}

int Song::durationInTicks() {
    this->joinTracks();

    if (this->getTrackCount() <= 0 || (*this)[0].size() == 0) {
        this->splitTracks();
        return 0;
    }

    int end_tick = (*this)[0].last().tick;
    this->splitTracks();
    return end_tick;
}

/**
 * Convert playback time into a song tick.
 */
int Song::getTickFromTime(double seconds) {
    if (seconds <= 0.0) return 0;

    int tpqn = this->getTicksPerQuarterNote();
    if (tpqn <= 0) return 0;

    double current_spt = 0.5 / static_cast<double>(tpqn);
    int prev_tick = 0;
    double remaining = seconds;

    // use the tick 0 tempo if one exists.
    if (!m_tempo_changes.isEmpty()) {
        auto it0 = m_tempo_changes.begin();
        if (it0.key() == 0 && it0.value()) {
            current_spt = it0.value()->getTempoSPT(tpqn);
        }
    }

    for (auto it = m_tempo_changes.begin(); it != m_tempo_changes.end(); ++it) {
        int tempo_tick = it.key();
        smf::MidiEvent *tempo_event = it.value();
        if (!tempo_event) continue;

        if (tempo_tick <= prev_tick) {
            current_spt = tempo_event->getTempoSPT(tpqn);
            continue;
        }

        int delta_ticks = tempo_tick - prev_tick;
        double segment_seconds = delta_ticks * current_spt;

        if (remaining < segment_seconds) {
            return prev_tick + static_cast<int>(std::floor(remaining / current_spt));
        }

        remaining -= segment_seconds;
        prev_tick = tempo_tick;
        current_spt = tempo_event->getTempoSPT(tpqn);
    }

    return prev_tick + static_cast<int>(std::floor(remaining / current_spt));
}

/**
 * Extract the initial per-channel state from tick 0 events.
 */
void Song::extractInitialState() {
    for (int i = 0; i < g_num_midi_channels; i++) {
        m_initial_programs[i] = 0;
        m_initial_channel_states[i] = InitialChannelState();
    }

    bool found_program[g_num_midi_channels] = {false};
    bool found_volume[g_num_midi_channels] = {false};
    bool found_pan[g_num_midi_channels] = {false};
    bool found_expression[g_num_midi_channels] = {false};
    bool found_pitch_bend[g_num_midi_channels] = {false};

    for (auto *track : m_events) {
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent &event = (*track)[i];
            if (event.tick > 0) break;

            uint8_t ch = event.getChannel();
            if (ch >= g_num_midi_channels) continue;

            if (event.isPatchChange() && !found_program[ch]) {
                m_initial_programs[ch] = event.getP1();
                found_program[ch] = true;
            }
            else if (event.isController()) {
                uint8_t cc = event.getP1();
                uint8_t value = event.getP2();

                if (cc == g_midi_cc_volume && !found_volume[ch]) {
                    m_initial_channel_states[ch].volume = value;
                    found_volume[ch] = true;
                }
                else if (cc == g_midi_cc_pan && !found_pan[ch]) {
                    m_initial_channel_states[ch].pan = value;
                    found_pan[ch] = true;
                }
                else if (cc == g_midi_cc_expression && !found_expression[ch]) {
                    m_initial_channel_states[ch].expression = value;
                    found_expression[ch] = true;
                }
            }
            else if (event.isPitchbend() && !found_pitch_bend[ch]) {
                int bend = (event.getP2() << 7) | event.getP1();
                m_initial_channel_states[ch].pitch_bend =
                    static_cast<int16_t>(bend - k_pitch_bend_center);
                found_pitch_bend[ch] = true;
            }
        }
    }
}
