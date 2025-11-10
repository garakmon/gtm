#pragma once
#ifndef PLAYER_H
#define PLAYER_H

extern "C" {
    #include "portaudio.h"
}

#include "mixer.h"



class Player {
public:
    Player() : m_audio_stream(nullptr) {}

    bool initializeAudio();

    Mixer *getMixer() { return &m_mixer; }

private:
    static int paStaticCallback(const void * input, void * output,
                                unsigned long frame_count,
                                const PaStreamCallbackTimeInfo * time_info,
                                PaStreamCallbackFlags status_flags,
                                void * user_data);

    PaStream *m_audio_stream;
    Mixer m_mixer;
};



#endif // PLAYER_H
