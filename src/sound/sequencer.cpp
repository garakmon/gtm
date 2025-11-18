
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

void Sequencer::seekToTick(int tick) {
    if (!m_song) {
        m_current_time = 0.0;
        m_event_index = 0;
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
