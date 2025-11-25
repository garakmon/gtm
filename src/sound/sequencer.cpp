
#include "sequencer.h"

#include "mixer.h"
#include "constants.h"



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

void Sequencer::reset() {
    m_current_time = 0.0;
    m_event_index = 0;
    m_loop_begin_index = -1;
    m_is_playing = false;

    // Use initial state extracted from track headers by the Song
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
    } else {
        for (int i = 0; i < g_num_midi_channels; ++i) {
            m_channel_program[i] = 0;
        }
    }
}

void Sequencer::seekToTick(int tick) {
    if (!m_song) {
        m_current_time = 0.0;
        m_event_index = 0;
        m_loop_begin_index = -1;
        return;
    }

    m_current_time = m_song->getTimeInSeconds(tick);

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

    // Start with initial state extracted from track headers
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

    // Apply changes up to seek point
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

            if (cc == 7) m_mixer->setChannelVolume(ch, value);
            else if (cc == 10) m_mixer->setChannelPan(ch, value);
            else if (cc == 11) m_mixer->setChannelExpression(ch, value);
        }
        else if (events[i]->isPitchbend() && m_mixer) {
            uint8_t ch = events[i]->getChannel();
            int bend = (events[i]->getP2() << 7) | events[i]->getP1();
            m_mixer->setChannelPitchBend(ch, bend);
        }
    }
}

void Sequencer::update(unsigned long frames) {
    if (!m_is_playing || !m_song || !m_mixer) return;

    double time_delta = static_cast<double>(frames) / g_sample_rate;
    m_current_time += time_delta;

    processEventsUpTo(m_current_time);
}

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

bool Sequencer::dispatchEvent(smf::MidiEvent *event) {
    uint8_t channel = event->getChannel();

    if (event->isNoteOn()) {
        uint8_t velocity = event->getVelocity();
        // Note-on with velocity 0 is equivalent to note-off in MIDI
        if (velocity == 0) {
            m_mixer->noteOff(channel, event->getKeyNumber());
        } else {
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
        case 7:   // Channel volume
            m_mixer->setChannelVolume(channel, value);
            break;
        case 10:  // Pan
            m_mixer->setChannelPan(channel, value);
            break;
        case 11:  // Expression
            m_mixer->setChannelExpression(channel, value);
            break;
        }
    }
    else if (event->isPitchbend()) {
        // Pitch bend is 14-bit: combine P1 (LSB) and P2 (MSB)
        int bend = (event->getP2() << 7) | event->getP1();
        m_mixer->setChannelPitchBend(channel, bend);
    }
    else if (event->isText() || event->isMarkerText()) {
        std::string text = event->getMetaContent();
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
