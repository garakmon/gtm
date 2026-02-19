#include "sound/sequencer.h"

#include "sound/mixer.h"
#include "util/constants.h"

#include <cstring>
#include <cmath>
#include <cstdint>



void Sequencer::setSong(Song *song) {
    m_song = song;
}

void Sequencer::setMixer(Mixer *mixer) {
    m_mixer = mixer;
}

void Sequencer::play() {
    m_is_playing = true;
}

void Sequencer::stop() {
    m_is_playing = false;
}

/**
 * Reset playback position, loop state, event index, and channel program state to the
 * start of the song, then reapplies the song’s initial channel settings and LFO rate.
 */
void Sequencer::reset() {
    m_current_time = 0.0;
    m_current_sample = 0;
    m_event_index = 0;
    m_loop_begin_index = -1;
    m_is_playing = false;

    // use initial state extracted from track headers by the Song
    if (m_song && m_mixer) {
        const uint8_t *initial_programs = m_song->getInitialPrograms();
        const Song::InitialChannelState *initial_states = m_song->getInitialChannelStates();

        for (int i = 0; i < g_num_midi_channels; ++i) {
            m_channel_program[i] = initial_programs[i];
            m_mixer->setChannelVolume(i, initial_states[i].volume);
            m_mixer->setChannelPan(i, initial_states[i].pan);
            m_mixer->setChannelExpression(i, initial_states[i].expression);
            m_mixer->setChannelPitchBend(i, initial_states[i].pitch_bend + 8192);
        }

        // set initial LFO tick rate from song tempo
        int tpqn = m_song->getTicksPerQuarterNote();
        if (tpqn > 0) {
            double spt = 0.5 / static_cast<double>(tpqn); // default = 500000 us/quarter
            // check for tempo at tick 0
            auto &tempo_changes = m_song->getTempoChanges();
            if (!tempo_changes.isEmpty()) {
                auto it = tempo_changes.begin();
                if (it.key() == 0 && it.value()) {
                    spt = it.value()->getTempoSPT(tpqn);
                }
            }
            int samples_per_tick = static_cast<int>(spt * g_sample_rate);
            if (samples_per_tick > 0) m_mixer->setLfoTickRate(samples_per_tick);
        }
    } else {
        for (int i = 0; i < g_num_midi_channels; ++i) {
            m_channel_program[i] = 0;
        }
    }
}

/**
 * Convert a target tick to absolute time, move playback to that point, restore initial
 * channel states, and replay prior state-changing events to seek position.
 */
void Sequencer::seekToTick(int tick) {
    if (!m_song) {
        m_current_time = 0.0;
        m_current_sample = 0;
        m_event_index = 0;
        m_loop_begin_index = -1;
        return;
    }

    m_current_time = m_song->getTimeInSeconds(tick);
    m_current_sample = static_cast<int64_t>(std::floor(m_current_time * g_sample_rate));

    auto &events = m_song->getMergedEvents();

    int lo = 0, hi = events.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (events[mid]->seconds < m_current_time) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    m_event_index = lo;

    m_loop_begin_index = -1;

    // start with initial state extracted from track headers
    const uint8_t *initial_programs = m_song->getInitialPrograms();
    const Song::InitialChannelState *initial_states = m_song->getInitialChannelStates();

    for (int i = 0; i < g_num_midi_channels; ++i) {
        m_channel_program[i] = initial_programs[i];
        if (m_mixer) {
            m_mixer->setChannelVolume(i, initial_states[i].volume);
            m_mixer->setChannelPan(i, initial_states[i].pan);
            m_mixer->setChannelExpression(i, initial_states[i].expression);
            m_mixer->setChannelPitchBend(i, initial_states[i].pitch_bend + 8192);
        }
    }

    // apply changes up to seek point
    for (int i = 0; i < m_event_index; ++i) {
        if (events[i]->isText() || events[i]->isMarkerText()) {
            if (events[i]->getMetaContent() == "[") {
                m_loop_begin_index = i;
            }
        }
        else if (events[i]->isPatchChange()) {
            uint8_t ch = events[i]->getChannel();
            m_channel_program[ch] = events[i]->getP1();
        }
        else if (events[i]->isController() && m_mixer) {
            uint8_t ch = events[i]->getChannel();
            uint8_t cc = events[i]->getP1();
            uint8_t value = events[i]->getP2();

            if      (cc == 1) m_mixer->setChannelMod(ch, value);
            else if (cc == 7) m_mixer->setChannelVolume(ch, value);
            else if (cc == 10) m_mixer->setChannelPan(ch, value);
            else if (cc == 11) m_mixer->setChannelExpression(ch, value);
            else if (cc == 21) m_mixer->setChannelLfos(ch, value);
            else if (cc == 22) m_mixer->setChannelModt(ch, value);
            else if (cc == 26) m_mixer->setChannelLfodl(ch, value);
        }
        else if (events[i]->isPitchbend() && m_mixer) {
            uint8_t ch = events[i]->getChannel();
            int bend = (events[i]->getP2() << 7) | events[i]->getP1();
            m_mixer->setChannelPitchBend(ch, bend);
        }
    }
}

/**
 * Advance playback time by a frame count and process all events up to the new time.
 * (does not render audio)
 */
void Sequencer::update(unsigned long frames) {
    if (!m_is_playing || !m_song || !m_mixer) return;

    double time_delta = static_cast<double>(frames) / g_sample_rate;
    m_current_time += time_delta;
    m_current_sample += static_cast<int64_t>(frames);

    this->processEventsUpTo(m_current_time);
}

/**
 * The main real-time playback routine.
 * 
 * Fill an output buffer by rendering audio in chunks between event boundaries,
 *  while dispatching events at exact sample positions.
 */
void Sequencer::fillAudio(float *out_buffer, unsigned long frames) {
    if (!out_buffer || frames == 0) return;

    // silence if not ready or not playing
    if (!m_is_playing || !m_song || !m_mixer) {
        std::memset(out_buffer, 0, frames * 2 * sizeof(float));
        return;
    }

    auto &events = m_song->getMergedEvents();
    unsigned long frames_remaining = frames;
    float *write_ptr = out_buffer;

    while (frames_remaining > 0) {
        // dispatch any events scheduled at or before current time
        while (m_event_index < events.size()) {
            double event_time = events[m_event_index]->seconds;
            int64_t event_sample = static_cast<int64_t>(
                std::floor(event_time * g_sample_rate)
            );
            if (event_sample > m_current_sample) break;
            if (dispatchEvent(events[m_event_index])) {
                // loop occurred
                m_current_sample = static_cast<int64_t>(
                    std::floor(m_current_time * g_sample_rate)
                );
                continue;
            }
            m_event_index++;
        }

        // Determine next event time or end of buffer
        int64_t buffer_end_sample = m_current_sample
                                  + static_cast<int64_t>(frames_remaining);
        int64_t next_event_sample = buffer_end_sample;

        if (m_event_index < events.size()) {
            int64_t event_sample = static_cast<int64_t>(
                std::floor(events[m_event_index]->seconds * g_sample_rate));
            if (event_sample < next_event_sample) {
                next_event_sample = event_sample;
            }
        }

        if (next_event_sample < m_current_sample) {
            next_event_sample = m_current_sample;
        }

        unsigned long frames_until_event = 0;
        if (next_event_sample > m_current_sample) {
            frames_until_event = static_cast<unsigned long>(
                next_event_sample - m_current_sample
            );
        }

        if (frames_until_event > frames_remaining) {
            frames_until_event = frames_remaining;
        }

        if (frames_until_event == 0) {
            if (m_event_index >= events.size()) {
                frames_until_event = frames_remaining;
            } else {
                // Advance to the event boundary without rendering audio.
                m_current_sample = next_event_sample;
                m_current_time = static_cast<double>(m_current_sample) / g_sample_rate;
                continue;
            }
        }

        m_mixer->processAudio(write_ptr, frames_until_event);
        write_ptr += frames_until_event * 2;
        frames_remaining -= frames_until_event;
        m_current_sample += static_cast<int64_t>(frames_until_event);
        m_current_time = static_cast<double>(m_current_sample) / g_sample_rate;
    }
}

/**
 * Dispatch all queued song events whose timestamps are at or before the given time.
 */
void Sequencer::processEventsUpTo(double time) {
    auto &events = m_song->getMergedEvents();

    while (m_event_index < events.size()) {
        smf::MidiEvent *event = events[m_event_index];

        if (event->seconds > time) break;

        if (dispatchEvent(event)) {
            break;
        }
        m_event_index++;
    }
}

/**
 * Interpret one MIDI event and apply the corresponding action to sequencer/mixer state.
 * Returning whether a loop jump occurred.
 */
bool Sequencer::dispatchEvent(smf::MidiEvent *event) {
    uint8_t channel = event->getChannel();

    if (event->isNoteOn()) {
        uint8_t velocity = event->getVelocity();
        // Note-on with velocity 0 is equivalent to note-off in MIDI
        if (velocity == 0) {
            m_mixer->noteOff(channel, event->getKeyNumber());
        } else {
            // Reset LFO delay on note-on (m4a behavior)
            m_mixer->resetLfo(channel);
            uint8_t program = m_channel_program[channel];
            m_mixer->noteOn(channel, event->getKeyNumber(), velocity, program);
        }
    }
    else if (event->isNoteOff()) {
        m_mixer->noteOff(channel, event->getKeyNumber());
    }
    else if (event->isPatchChange()) {
        m_channel_program[channel] = event->getP1();
    }
    else if (event->isController()) {
        uint8_t cc = event->getP1();
        uint8_t value = event->getP2();

        switch (cc) {
        case 1:   // modulation depth (MOD)
            m_mixer->setChannelMod(channel, value);
            break;
        case 7:   // channel volume
            m_mixer->setChannelVolume(channel, value);
            break;
        case 10:  // pan
            m_mixer->setChannelPan(channel, value);
            break;
        case 11:  // expression
            m_mixer->setChannelExpression(channel, value);
            break;
        case 21:  // LFO speed (LFOS)
            m_mixer->setChannelLfos(channel, value);
            break;
        case 22:  // modulation type (MODT)
            m_mixer->setChannelModt(channel, value);
            break;
        case 26:  // LFO delay (LFODL)
            m_mixer->setChannelLfodl(channel, value);
            break;
        }
    }
    else if (event->isPitchbend()) {
        // pitch bend is 14-bit: combine P1 (LSB) and P2 (MSB)
        int bend = (event->getP2() << 7) | event->getP1();
        m_mixer->setChannelPitchBend(channel, bend);
    }
    else if (event->isTempo() && m_mixer) {
        // update LFO tick rate when tempo changes
        int tpqn = m_song ? m_song->getTicksPerQuarterNote() : 24;
        double spt = event->getTempoSPT(tpqn);
        int samples_per_tick = static_cast<int>(spt * g_sample_rate);
        if (samples_per_tick > 0) m_mixer->setLfoTickRate(samples_per_tick);
    }
    else if (event->isText() || event->isMarkerText()) {
        std::string text = event->getMetaContent();
        // "[]" are text loop markers
        if (text == "[") {
            m_loop_begin_index = m_event_index;
        }
        else if (text == "]" && m_loop_begin_index >= 0) {
            m_event_index = m_loop_begin_index;
            m_current_time = m_song->getMergedEvents()[m_loop_begin_index]->seconds;
            m_mixer->end();
            return true;
        }
    }
    return false;
}
