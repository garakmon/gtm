#pragma once
#ifndef MIXER_H
#define MIXER_H

extern "C" {
    #include "portaudio.h"
}

#include "soundtypes.h"
#include "constants.h"

class Mixer {
public:
    Mixer() {}

    void noteOn(uint8_t channel, uint8_t key, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t key);
    void processAudio(float *out_buffer, unsigned long frame_count);
    void end();

private:
    Voice m_voices[g_max_voices];

    Voice *allocateVoice(uint8_t key);
};

#endif // MIXER_H
