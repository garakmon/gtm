
#include "mixer.h"

#include <cmath>



float Voice::getNextSample() {
    if (!is_active) return 0.0f;

    float sample = 0.0f;

    if (sample_data && sample_length > 0) {
        int idx = static_cast<int>(position);

        if (idx >= static_cast<int>(sample_length) - 1) {
            if (loops && loop_end > loop_start) {
                position = loop_start + std::fmod(position - loop_end, loop_end - loop_start);
                idx = static_cast<int>(position);
            } else {
                is_active = false;
                return 0.0f;
            }
        }

        float frac = position - idx;
        float s0 = sample_data[idx] / 128.0f;
        float s1 = sample_data[idx + 1] / 128.0f;
        sample = s0 + (s1 - s0) * frac;

        position += pitch_ratio;
    }

    if (releasing) {
        amplitude -= release_rate;
        if (amplitude <= 0.0f) {
            is_active = false;
            return 0.0f;
        }
    }

    return sample * amplitude * (velocity / 127.0f);
}

void Voice::noteOn(uint8_t ch, uint8_t key, uint8_t vel, const Sample *sample, int base_key) {
    channel = ch;
    midi_key = key;
    velocity = vel;
    is_active = true;
    releasing = false;
    position = 0.0;

    if (sample && !sample->data.isEmpty()) {
        sample_data = reinterpret_cast<const int8_t *>(sample->data.constData());
        sample_length = sample->data.size();
        loop_start = sample->loop_start;
        loop_end = sample->loop_end > 0 ? sample->loop_end : sample_length;
        loops = sample->loops;

        double freq_ratio = std::pow(2.0, (key - base_key) / 12.0);
        pitch_ratio = (static_cast<double>(sample->sample_rate) / g_sample_rate) * freq_ratio;
    } else {
        sample_data = nullptr;
        sample_length = 0;
        pitch_ratio = 1.0;
    }

    amplitude = 0.3f;
    release_rate = 0.001f;
}

void Voice::noteOff() {
    releasing = true;
}



void Mixer::setInstrumentData(const VoiceGroup *vg, const QMap<QString, Sample> *samples) {
    m_voicegroup = vg;
    m_samples = samples;
}

Voice *Mixer::allocateVoice(uint8_t key) {
    // !TODO: improve this logic to be gba accurate
    // --should use priority (from SongEntry), then envelope (ASDR)
    // look for an inactive voice
    for (int i = 0; i < g_max_voices; i++) {
        if (!m_voices[i].is_active) {
            return &m_voices[i];
        }
    }

    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].releasing) {
            return &m_voices[i];
        }
    }

    return &m_voices[0];
}

void Mixer::noteOn(uint8_t channel, uint8_t key, uint8_t velocity, uint8_t program) {
    const Sample *sample = nullptr;
    int base_key = g_midi_middle_c;

    if (m_voicegroup && m_samples && program < m_voicegroup->instruments.size()) {
        const Instrument &inst = m_voicegroup->instruments[program];
        base_key = inst.base_key;

        if (!inst.sample_label.isEmpty()) {
            auto it = m_samples->find(inst.sample_label);
            if (it != m_samples->end()) {
                sample = &it.value();
            }
        }
    }

    Voice *voice = allocateVoice(key);
    voice->noteOn(channel, key, velocity, sample, base_key);
}

void Mixer::noteOff(uint8_t channel, uint8_t key) {
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].is_active && !m_voices[i].releasing &&
            m_voices[i].channel == channel && m_voices[i].midi_key == key) {
            m_voices[i].noteOff();
            return;
        }
    }
}

void Mixer::processAudio(float *out_buffer, unsigned long frame_count) {
    constexpr float master_volume = 0.25f;  // !TODO: use the slider

    for (unsigned long i = 0; i < frame_count; i++) {
        float mixed_sample = 0.0f;

        for (int v = 0; v < g_max_voices; v++) {
            if (m_voices[v].is_active) {
                mixed_sample += m_voices[v].getNextSample();
            }
        }

        mixed_sample *= master_volume;

        // clip the volume so it doesnt blow out my speakers
        if (mixed_sample > 1.0f) mixed_sample = 1.0f;
        else if (mixed_sample < -1.0f) mixed_sample = -1.0f;

        *out_buffer++ = mixed_sample;
        *out_buffer++ = mixed_sample;
    }
}

void Mixer::end() {
    for (int i = 0; i < g_max_voices; i++) {
        m_voices[i].is_active = false;
        m_voices[i].releasing = false;
    }
}
