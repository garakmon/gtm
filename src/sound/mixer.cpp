
#include "mixer.h"


int Mixer::audioCallback(void * output_buffer, uint32_t frames_requested) {
    float * out_ptr = static_cast<float *>(output_buffer);

    for (uint32_t i = 0x00; i < frames_requested; ++i) {
        float sample = 0.0f;

        if (m_is_debug_playing) {
            // A simple 440Hz square wave for testing
            // 44100 / 440 = ~100 samples per cycle
            sample = (m_phase_counter < 50) ? 0.05f : -0.05f;
            m_phase_counter = (m_phase_counter + 1) % 100;
        }

        out_ptr[i * 0x02] = sample;
        out_ptr[i * 0x02 + 0x01] = sample;
    }

    return paContinue;
}
