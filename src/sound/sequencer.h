#pragma once
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "song.h"

class Mixer;

class Sequencer {
public:
    void setSong(Song *song);
    void setMixer(Mixer *mixer);

    void play();
    void stop();
    void reset();
    void seekToTick(int tick);

    void update(unsigned long frames);

    bool isPlaying() const { return m_is_playing; }
    double getCurrentTime() const { return m_current_time; }

private:
    Song *m_song = nullptr;
    Mixer *m_mixer = nullptr;

    bool m_is_playing = false;
    double m_current_time = 0.0;
    int m_event_index = 0;

    void processEventsUpTo(double time);
    void dispatchEvent(smf::MidiEvent *event);
};

#endif // SEQUENCER_H
