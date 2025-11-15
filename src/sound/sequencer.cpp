
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
    m_is_playing = false;
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

        dispatchEvent(event);
        m_event_index++;
    }
}

void Sequencer::dispatchEvent(smf::MidiEvent *event) {
    if (event->isNoteOn()) {
        m_mixer->noteOn(event->getChannel(), event->getKeyNumber(), event->getVelocity());
    }
    else if (event->isNoteOff()) {
        m_mixer->noteOff(event->getChannel(), event->getKeyNumber());
    }
}
