#pragma once
#ifndef MIXER_H
#define MIXER_H


extern "C" {
    #include "portaudio.h"
}

#include <algorithm>

class Mixer {
public:
    Mixer() {}

    int audioCallback(void * output_buffer, uint32_t frames_requested);

    void toggleDebugNote(bool play) {
        this->is_debug_playing = play;
    }

private:
    bool m_is_debug_playing = false;
    int m_phase_counter = 0;
};


#endif // MIXER_H
