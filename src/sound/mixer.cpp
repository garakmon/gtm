
#include "mixer.h"

#include <cmath>



float Voice::getNextSample() {
    if (!is_active) return 0.0f;

    // Square wave
    float sample = (phase < 0.5) ? amplitude : -amplitude;

    phase += phase_increment;
    if (phase >= 1.0) phase -= 1.0;

    // Simple release envelope
    if (releasing) {
        amplitude *= 0.995f;
        if (amplitude < 0.001f) is_active = false;
    }

    return sample;
}

void Voice::noteOn(uint8_t key, uint8_t vel) {
    midi_key = key;
    velocity = vel;
    is_active = true;
    releasing = false;

    // MIDI key 69 = A4 = 440Hz
    double freq = 440.0 * std::pow(2.0, (key - 69) / 12.0);
    phase_increment = freq / g_sample_rate;
    phase = 0.0;

    amplitude = (vel / 127.0f) * 0.1f;
}

void Voice::noteOff() {
    releasing = true;
}



Voice *Mixer::allocateVoice(uint8_t key) {
    // !TODO: improve this logic to be gba accurate
    // should use priority (from SongEntry), then envelope (ASDR)
    // look for an inactive voice
    for (int i = 0; i < g_max_voices; i++) {
        if (!m_voices[i].is_active) {
            return &m_voices[i];
        }
    }

    // found none => steal the first releasing voice
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].releasing) {
            return &m_voices[i];
        }
    }

    // releasing voices => steal the first one
    return &m_voices[0];
}

void Mixer::noteOn(uint8_t channel, uint8_t key, uint8_t velocity) {
    (void)channel; // unused for now

    Voice *voice = allocateVoice(key);
    voice->noteOn(key, velocity);
}

void Mixer::noteOff(uint8_t channel, uint8_t key) {
    (void)channel; // unused for now

    // find voice with matching key and trigger release
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].is_active && !m_voices[i].releasing && m_voices[i].midi_key == key) {
            m_voices[i].noteOff();
            return;
        }
    }
}

void Mixer::processAudio(float *out_buffer, unsigned long frame_count) {
    for (unsigned long i = 0; i < frame_count; i++) {
        float mixed_sample = 0.0f;

        for (int v = 0; v < g_max_voices; v++) {
            if (m_voices[v].is_active) {
                mixed_sample += m_voices[v].getNextSample();
            }
        }

        // write to interleaved stereo buffer
        *out_buffer++ = mixed_sample;
        *out_buffer++ = mixed_sample;
    }
}

void Mixer::end() {
    // kill all the voices
    for (int i = 0; i < g_max_voices; i++) {
        m_voices[i].is_active = false;
        m_voices[i].releasing = false;
    }
}
