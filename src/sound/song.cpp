#include "song.h"


#include <algorithm>
#include <QDebug>
#include <cmath>



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

// TIME SIGNATURE CODE
/*
    // load meta events from the first track
    for (int i = 0; i < midifile[0].size(); i++) {
        if (midifile[0][i].isTimeSignature()) {
            TimeSigChange change;
            change.tick = midifile[0][i].tick;
            change.num = midifile[0][i].getP1();
            change.den = std::pow(2, midifile[0][i].getP2());
            timeSigMap.push_back(change);
        }
    }
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

    // Extract initial state from track headers before merging
    this->extractInitialState();

    int track_num = 0;
    for (auto track : this->tracks()) {
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

void Song::mergeEvents() {
    // !TODO: whenever I get to letting edits happen, this probably needs
    // to be called whenever a Song is unclean/has been edited in order to play it back
    this->m_merged_events.clear();

    int track_count = this->getTrackCount();

    int total_events = 0;
    for (int t = 0; t < track_count; t++) {
        total_events += this->getEventCount(t);
    }

    this->m_merged_events.reserve(total_events);

    for (int track_index = 0; track_index < track_count; track_index++) {
        smf::MidiEventList &event_list = *this->m_events[track_index];
        int event_count = event_list.size();

        for (int i = 0; i < event_count; ++i) {
            this->m_merged_events.append(&event_list[i]);
        }
    }

    // Preserve event order for identical ticks (important for note on/off ordering)
    std::stable_sort(this->m_merged_events.begin(), this->m_merged_events.end(),
        [](smf::MidiEvent *a, smf::MidiEvent *b) {
            return a->tick < b->tick;
        }
    );
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

int Song::getTickFromTime(double seconds) {
    if (seconds <= 0.0) return 0;

    // Tempo-aware time -> tick conversion to avoid drift on tempo changes.
    int tpqn = this->getTicksPerQuarterNote();
    if (tpqn <= 0) return 0;

    static int debug_count = 0;

    // Default MIDI tempo is 120 BPM = 500000 us per quarter note.
    double current_spt = 0.5 / static_cast<double>(tpqn);
    int prev_tick = 0;
    double remaining = seconds;

    // If there is a tempo at tick 0, use it as initial tempo.
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
            int tick = prev_tick + static_cast<int>(std::floor(remaining / current_spt));
            if (debug_count < 8) {
                qDebug() << "getTickFromTime:" << seconds << "->" << tick
                         << "(tempo-aware, remaining segment)";
                debug_count++;
            }
            return tick;
        }

        remaining -= segment_seconds;
        prev_tick = tempo_tick;
        current_spt = tempo_event->getTempoSPT(tpqn);
    }

    // After last tempo change.
    int tick = prev_tick + static_cast<int>(std::floor(remaining / current_spt));
    if (debug_count < 8) {
        qDebug() << "getTickFromTime:" << seconds << "->" << tick
                 << "(tempo-aware, after last tempo)";
        debug_count++;
    }
    return tick;
}

void Song::extractInitialState() {
    // Initialize defaults
    for (int i = 0; i < 16; i++) {
        m_initial_programs[i] = 0;
        m_initial_channel_states[i] = InitialChannelState();
    }

    // Track which values we've found per channel
    bool found_program[16] = {false};
    bool found_volume[16] = {false};
    bool found_pan[16] = {false};
    bool found_expression[16] = {false};
    bool found_pitch_bend[16] = {false};

    // For each track, extract initial state from events at tick 0
    // These are the "setup" events before the music starts
    for (auto *track : this->m_events) {
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent &event = (*track)[i];

            // Only consider events at tick 0 as initial state
            if (event.tick > 0) break;

            uint8_t ch = event.getChannel();
            if (ch >= 16) continue;

            if (event.isPatchChange() && !found_program[ch]) {
                m_initial_programs[ch] = event.getP1();
                found_program[ch] = true;
            }
            else if (event.isController()) {
                uint8_t cc = event.getP1();
                uint8_t value = event.getP2();

                if (cc == 7 && !found_volume[ch]) {
                    m_initial_channel_states[ch].volume = value;
                    found_volume[ch] = true;
                }
                else if (cc == 10 && !found_pan[ch]) {
                    m_initial_channel_states[ch].pan = value;
                    found_pan[ch] = true;
                }
                else if (cc == 11 && !found_expression[ch]) {
                    m_initial_channel_states[ch].expression = value;
                    found_expression[ch] = true;
                }
            }
            else if (event.isPitchbend() && !found_pitch_bend[ch]) {
                int bend = (event.getP2() << 7) | event.getP1();
                m_initial_channel_states[ch].pitch_bend = static_cast<int16_t>(bend - 8192);
                found_pitch_bend[ch] = true;
            }
        }
    }
}
