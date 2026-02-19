#pragma once
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "sound/song.h"



class Mixer;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// The Sequencer is the timeline of the audio engine.
/// It owns the current playback position in time, knows which song event comes next,
/// and applies events to the Mixer at the exact moments the audio callback reaches them.
///
/// This runs in sample time and not tick time, meaning playback is advanced according to
/// exact audio sample positions and events are applied at precise buffer boundaries
/// (rather than stepping through the song in midi ticks)
///
//////////////////////////////////////////////////////////////////////////////////////////
class Sequencer {
public:
    void setSong(Song *song);
    void setMixer(Mixer *mixer);

    void play();
    void stop();
    void reset();
    void seekToTick(int tick);

    void update(unsigned long frames);
    void fillAudio(float *out_buffer, unsigned long frames);

    bool isPlaying() const { return m_is_playing; }
    double getCurrentTime() const { return m_current_time; }

private:
    void processEventsUpTo(double time);
    bool dispatchEvent(smf::MidiEvent *event);

private:
    // non-owning references
    Song *m_song = nullptr;
    Mixer *m_mixer = nullptr;

    // playback state info
    bool m_is_playing = false;
    double m_current_time = 0.0;  // in seconds
    int64_t m_current_sample = 0; // position in output samples, used in fillAudio()
    int m_event_index = 0;        // index into the merged song event vector
    int m_loop_begin_index = -1;

    uint8_t m_channel_program[g_num_midi_channels] = {0}; // program number per channel
};

#endif // SEQUENCER_H
