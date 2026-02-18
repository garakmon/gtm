#pragma once
#ifndef PLAYER_H
#define PLAYER_H

extern "C" {
    #include "portaudio.h"
}

#include "sound/mixer.h"
#include "sound/sequencer.h"



class Song;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// The Player is the audio playback engine, which coordinates the Controller
/// (current song, playhead position, controls) with the sound engine. This class
/// is what advances a song through time, via the PortAudio callback.
///
/// With the Mixer, Player translates song events into mixer state changes.
/// (eg. note on/note off, program change, control change, etc)
///
//////////////////////////////////////////////////////////////////////////////////////////
class Player {
public:
    Player() : m_audio_stream(nullptr) {}
    ~Player();

    bool initializeAudio();

    void loadSong(Song *song, const VoiceGroup *vg,
                  const QMap<QString, VoiceGroup> *all_vgs,
                  const QMap<QString, Sample> *samples,
                  const QMap<QString, QByteArray> *pcm_data,
                  const QMap<QString, KeysplitTable> *keysplit_tables);
    void play();
    void stop();
    void seekToTick(int tick);

    Mixer *getMixer() { return &m_mixer; }
    Sequencer *getSequencer() { return &m_sequencer; }

private:
    void audioCallback(float *out_buffer, unsigned long frame_count);
    static int paStaticCallback(const void *input, void *output,
                                unsigned long frame_count,
                                const PaStreamCallbackTimeInfo *time_info,
                                PaStreamCallbackFlags status_flags,
                                void *user_data);

    PaStream *m_audio_stream;

    Mixer m_mixer;
    Sequencer m_sequencer;
};



#endif // PLAYER_H
