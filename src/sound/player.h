#pragma once
#ifndef PLAYER_H
#define PLAYER_H

extern "C" {
    #include "portaudio.h"
}

#include "mixer.h"
#include "sequencer.h"

class Song;

class Player {
public:
    Player() : m_audio_stream(nullptr) {}
    ~Player();

    bool initializeAudio();

    void loadSong(Song *song, const VoiceGroup *vg,
                  const QMap<QString, VoiceGroup> *all_vg,
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
    static int paStaticCallback(const void * input, void * output,
                                unsigned long frame_count,
                                const PaStreamCallbackTimeInfo * time_info,
                                PaStreamCallbackFlags status_flags,
                                void * user_data);

    PaStream *m_audio_stream;
    Mixer m_mixer;
    Sequencer m_sequencer;
};



#endif // PLAYER_H
