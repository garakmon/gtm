
#include "mixer.h"

#include <cmath>



// Duty cycle thresholds for square waves (12.5%, 25%, 50%, 75%)
static const double duty_thresholds[4] = {0.125, 0.25, 0.5, 0.75};

float Voice::getNextSample(float pitch_bend_multiplier) {
    if (!is_active) return 0.0f;

    float sample = 0.0f;

    switch (voice_type) {
    case 0x00:  // DirectSound
    case 0x08:  // DirectSound (no resample)
    case 0x10:  // DirectSound (alt)
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

            position += pitch_ratio * pitch_bend_multiplier;
        }
        break;

    case 0x01:  // Square1
    case 0x09:  // Square1 (alt)
    case 0x02:  // Square2
    case 0x0A:  // Square2 (alt)
        {
            double threshold = duty_thresholds[duty_cycle & 0x03];
            sample = (phase < threshold) ? 0.5f : -0.5f;

            phase += phase_inc * pitch_bend_multiplier;
            if (phase >= 1.0) phase -= 1.0;
        }
        break;

    case 0x03:  // ProgrammableWave
    case 0x0B:  // ProgrammableWave (alt)
        if (wave_data) {
            // 32 bytes = 16 4-bit samples, each byte has 2 samples
            int wave_idx = static_cast<int>(phase * 32.0) % 32;
            uint8_t byte = wave_data[wave_idx / 2];
            // High nibble first, then low nibble
            uint8_t nibble = (wave_idx & 1) ? (byte & 0x0F) : (byte >> 4);
            // Convert 0-15 to -1.0 to 1.0
            sample = (nibble / 7.5f) - 1.0f;

            phase += phase_inc * pitch_bend_multiplier;
            if (phase >= 1.0) phase -= 1.0;
        }
        break;

    case 0x04:  // Noise
    case 0x0C:  // Noise (alt)
        {
            // Clock the LFSR based on noise_period
            noise_counter++;
            if (noise_counter >= noise_period) {
                noise_counter = 0;
                // 15-bit LFSR with taps at bits 0 and 1 (GBA style)
                uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
                lfsr = (lfsr >> 1) | (bit << 14);
            }
            sample = (lfsr & 1) ? 0.5f : -0.5f;
        }
        break;

    default:
        break;
    }

    // Process ADSR envelope
    updateEnvelope();

    if (!is_active) return 0.0f;

    return sample * envelope * (velocity / 127.0f);
}

void Voice::noteOn(uint8_t ch, uint8_t key, uint8_t vel, const Instrument *inst,
                   const Sample *sample, const uint8_t *wave) {
    channel = ch;
    midi_key = key;
    velocity = vel;
    is_active = true;
    releasing = false;
    position = 0.0;
    phase = 0.0;

    int base_key = inst ? inst->base_key : g_midi_middle_c;
    voice_type = inst ? inst->type_id : 0;

    // Calculate frequency for synthesized voices
    // MIDI note to frequency: f = 440 * 2^((note - 69) / 12)
    double frequency = 440.0 * std::pow(2.0, (key - 69) / 12.0);
    phase_inc = frequency / g_sample_rate;

    switch (voice_type) {
    case 0x00:  // DirectSound
    case 0x08:  // DirectSound (no resample)
    case 0x10:  // DirectSound (alt)
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
        break;

    case 0x01:  // Square1
    case 0x09:  // Square1 (alt)
    case 0x02:  // Square2
    case 0x0A:  // Square2 (alt)
        duty_cycle = inst ? inst->duty_cycle : 2;
        break;

    case 0x03:  // ProgrammableWave
    case 0x0B:  // ProgrammableWave (alt)
        wave_data = wave;
        break;

    case 0x04:  // Noise
    case 0x0C:  // Noise (alt)
        lfsr = 0x7FFF;
        noise_counter = 0;
        // Higher notes = faster noise, lower = slower
        // Period based on MIDI key (this is an approximation)
        noise_period = std::max(1, 128 - key);
        break;

    default:
        break;
    }

    // Set up ADSR envelope from instrument parameters
    // GBA values: attack/decay/release 0-255 (higher = slower), sustain 0-15
    int attack = inst ? inst->attack : 0;
    int decay = inst ? inst->decay : 0;
    int sustain = inst ? inst->sustain : 15;
    int release = inst ? inst->release : 0;

    // Convert to per-sample rates
    // Scale factor tuned for reasonable envelope times at 44100 Hz
    constexpr float rate_scale = 1.0f / 256.0f;

    // Attack: 0 = instant, 255 = ~1.5 seconds
    attack_rate = (attack == 0) ? 1.0f : rate_scale / (attack + 1);

    // Decay: 0 = instant, 255 = ~1.5 seconds
    decay_rate = (decay == 0) ? 1.0f : rate_scale / (decay + 1);

    // Sustain: 0 = silent, 15 = full volume
    sustain_level = sustain / 15.0f;

    // Release: 0 = instant, 255 = ~1.5 seconds
    release_rate = (release == 0) ? 1.0f : rate_scale / (release + 1);

    // Start envelope at attack phase
    env_phase = ENV_ATTACK;
    envelope = 0.0f;
}

void Voice::noteOff() {
    releasing = true;
    env_phase = ENV_RELEASE;
}

void Voice::updateEnvelope() {
    switch (env_phase) {
    case ENV_ATTACK:
        envelope += attack_rate;
        if (envelope >= 1.0f) {
            envelope = 1.0f;
            env_phase = ENV_DECAY;
        }
        break;

    case ENV_DECAY:
        envelope -= decay_rate;
        if (envelope <= sustain_level) {
            envelope = sustain_level;
            env_phase = ENV_SUSTAIN;
        }
        break;

    case ENV_SUSTAIN:
        // Hold at sustain level until note off
        envelope = sustain_level;
        break;

    case ENV_RELEASE:
        envelope -= release_rate;
        if (envelope <= 0.0f) {
            envelope = 0.0f;
            is_active = false;
        }
        break;
    }
}



void Mixer::setInstrumentData(const VoiceGroup *vg, const QMap<QString, VoiceGroup> *all_vg,
                              const QMap<QString, Sample> *samples,
                              const QMap<QString, QByteArray> *pcm_data) {
    m_voicegroup = vg;
    m_all_voicegroups = all_vg;
    m_samples = samples;
    m_pcm_data = pcm_data;

    // Reset channel state to defaults
    for (int i = 0; i < g_num_midi_channels; i++) {
        m_channels[i].volume = 100;      // default MIDI volume
        m_channels[i].expression = 127;  // default expression (full)
        m_channels[i].pan = 64;          // center pan
        m_channels[i].pitch_bend = 0;    // no bend
    }
}

const Instrument *Mixer::resolveInstrument(const Instrument *inst, uint8_t key,
                                           const VoiceGroup **out_vg) const {
    if (!inst || !m_all_voicegroups) return inst;

    // Handle keysplit instruments (drums, multi-sampled instruments)
    if (inst->type_id == 0x40 || inst->type_id == 0x80) {
        // sample_label contains the name of the referenced voicegroup
        auto it = m_all_voicegroups->find(inst->sample_label);
        if (it != m_all_voicegroups->end()) {
            const VoiceGroup &drum_vg = it.value();
            int drum_index = key - drum_vg.offset;

            if (drum_index >= 0 && drum_index < drum_vg.instruments.size()) {
                if (out_vg) *out_vg = &drum_vg;
                // Recursively resolve in case of nested keysplits
                return resolveInstrument(&drum_vg.instruments[drum_index], key, out_vg);
            }
        }
        return nullptr;  // couldn't resolve
    }

    return inst;
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
    const Instrument *inst = nullptr;
    const Sample *sample = nullptr;
    const uint8_t *wave_data = nullptr;

    if (m_voicegroup && program < m_voicegroup->instruments.size()) {
        inst = &m_voicegroup->instruments[program];

        // Resolve keysplit/drumset instruments to actual instrument
        const VoiceGroup *resolved_vg = nullptr;
        inst = resolveInstrument(inst, key, &resolved_vg);

        if (!inst) return;  // couldn't resolve instrument

        // Look up sample for DirectSound instruments
        if (m_samples && !inst->sample_label.isEmpty()) {
            auto it = m_samples->find(inst->sample_label);
            if (it != m_samples->end()) {
                sample = &it.value();
            }
        }

        // Look up wave data for ProgrammableWave instruments
        if (m_pcm_data && !inst->sample_label.isEmpty()) {
            auto it = m_pcm_data->find(inst->sample_label);
            if (it != m_pcm_data->end()) {
                wave_data = reinterpret_cast<const uint8_t *>(it.value().constData());
            }
        }
    }

    Voice *voice = allocateVoice(key);
    voice->noteOn(channel, key, velocity, inst, sample, wave_data);
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

void Mixer::setChannelVolume(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].volume = value;
    }
}

void Mixer::setChannelPan(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].pan = value;
    }
}

void Mixer::setChannelExpression(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].expression = value;
    }
}

void Mixer::setChannelPitchBend(uint8_t channel, int value) {
    if (channel < g_num_midi_channels) {
        // Store as signed offset from center (8192)
        m_channels[channel].pitch_bend = static_cast<int16_t>(value - 8192);
    }
}

void Mixer::processAudio(float *out_buffer, unsigned long frame_count) {
    constexpr float master_volume = 0.25f;  // !TODO: use the slider

    for (unsigned long i = 0; i < frame_count; i++) {
        float mixed_sample = 0.0f;

        for (int v = 0; v < g_max_voices; v++) {
            if (m_voices[v].is_active) {
                uint8_t ch = m_voices[v].channel;

                // Calculate pitch bend multiplier (±2 semitones range)
                // pitch_bend is -8192 to +8191, map to ±2 semitones
                float bend_semitones = (m_channels[ch].pitch_bend / 8192.0f) * 2.0f;
                float pitch_multiplier = std::pow(2.0f, bend_semitones / 12.0f);

                float sample = m_voices[v].getNextSample(pitch_multiplier);

                // Apply channel volume and expression
                float ch_vol = m_channels[ch].volume / 127.0f;
                float ch_exp = m_channels[ch].expression / 127.0f;
                sample *= ch_vol * ch_exp;

                mixed_sample += sample;
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
