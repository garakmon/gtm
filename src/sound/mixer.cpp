
#include "mixer.h"

#include <cmath>
#include <algorithm>
#include <QDebug>



// Duty cycle thresholds for square waves (12.5%, 25%, 50%, 75%)
static const double duty_thresholds[4] = {0.125, 0.25, 0.5, 0.75};

// Helper to get voice type abbreviation (agbplay-style, pokeemerald-relevant)
static QString voiceTypeAbbrev(const Instrument *inst) {
    if (!inst) return "-";

    static const QMap<int, QString> baseType = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> dutyMap = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    const int type_id = inst->type_id;
    if (baseType.contains(type_id)) {
        return baseType.value(type_id);
    }

    const QString duty = dutyMap.value(inst->duty_cycle & 0x03, "50");

    switch (type_id) {
    case 0x01: // Square1 (sweep)
    case 0x09:
        return QString("Sq.%1S").arg(duty);
    case 0x02: // Square2
    case 0x0A:
        return QString("Sq.%1").arg(duty);
    case 0x04: // Noise
    case 0x0C:
        return (inst->duty_cycle & 0x1) ? "Ns.7" : "Ns.15";
    default:
        return "Multi";
    }
}

Mixer::Mixer() {
    for (int i = 0; i < g_num_midi_channels; ++i) {
        m_channel_peak_l[i].store(0.0f, std::memory_order_relaxed);
        m_channel_peak_r[i].store(0.0f, std::memory_order_relaxed);
    }
}

float Voice::getNextSample(float pitch_bend_multiplier) {
    if (!is_active) return 0.0f;

    float sample = 0.0f;

    switch (voice_type) {
    case 0x00:  // DirectSound
    case 0x08:  // DirectSound (no resample)
    case 0x10:  // DirectSound (alt)
        if (sample_data && sample_length > 0) {
            // GBA uses nearest-neighbor (point) sampling, no interpolation
            int idx = static_cast<int>(position);

            if (loops && loop_end > loop_start) {
                // Looping sample: wrap position within loop region
                if (idx >= static_cast<int>(loop_end)) {
                    position = loop_start + std::fmod(position - loop_start, loop_end - loop_start);
                    idx = static_cast<int>(position);
                }
            } else {
                // Non-looping sample: check for end
                if (idx >= static_cast<int>(sample_length)) {
                    is_active = false;
                    return 0.0f;
                }
            }

            // Point sampling (GBA-accurate)
            sample = sample_data[idx] / 128.0f;

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
            // Clock the LFSR based on noise_period (Galois form)
            if (++noise_counter >= noise_period) {
                noise_counter = 0;
                if (lfsr & 1) {
                    lfsr >>= 1;
                    lfsr ^= noise_mask;
                } else {
                    lfsr >>= 1;
                }
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

    // Convert envelope level to float (PSG: 0-15, DS: 0-255)
    float env_float = is_psg ? (env_level / 15.0f) : (env_level / 255.0f);
    return sample * env_float * (velocity / 127.0f);
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
    is_psg = false;

    int base_key = inst ? inst->base_key : g_midi_middle_c;
    voice_type = inst ? inst->type_id : 0;
    inst_pan_enabled = (inst && inst->pan != 0);
    inst_pan = inst_pan_enabled ? static_cast<int8_t>(std::clamp(inst->pan, -128, 127)) : 0;

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
        is_psg = true;
        duty_cycle = inst ? inst->duty_cycle : 2;
        break;

    case 0x03:  // ProgrammableWave
    case 0x0B:  // ProgrammableWave (alt)
        is_psg = true;
        wave_data = wave;
        break;

    case 0x04:  // Noise
    case 0x0C:  // Noise (alt)
        is_psg = true;
        {
            const int instr_np = inst ? inst->duty_cycle : 0; // noise period/mode param
            if (instr_np & 0x1) {
                lfsr = 0x40;        // 7-bit LFSR
                noise_mask = 0x60; // taps for 7-bit
            } else {
                lfsr = 0x4000;      // 15-bit LFSR
                noise_mask = 0x6000; // taps for 15-bit
            }
        }
        noise_counter = 0;
        // Approximate GBA noise pitch behavior (agbplay-style)
        {
            float fkey = static_cast<float>(key);
            float noisefreq;
            if (fkey < 76.0f) {
                noisefreq = 4096.0f * std::pow(8.0f, (fkey - 60.0f) * (1.0f / 12.0f));
            } else if (fkey < 78.0f) {
                noisefreq = 65536.0f * std::pow(2.0f, (fkey - 76.0f) * (1.0f / 2.0f));
            } else if (fkey < 80.0f) {
                noisefreq = 131072.0f * std::pow(2.0f, (fkey - 78.0f));
            } else {
                noisefreq = 524288.0f;
            }
            if (noisefreq < 4.5714f) noisefreq = 4.5714f;
            noise_period = std::max(1, static_cast<int>(std::round(g_sample_rate / noisefreq)));
        }
        break;

    default:
        break;
    }

    // Set up ADSR envelope
    if (is_psg) {
        // PSG envelopes use small integer steps (0-7 / 0-15)
        env_attack = inst ? static_cast<uint8_t>(inst->attack & 0x07) : 0;
        env_decay = inst ? static_cast<uint8_t>(inst->decay & 0x07) : 0;
        env_sustain = inst ? static_cast<uint8_t>(inst->sustain & 0x0F) : 15;
        env_release = inst ? static_cast<uint8_t>(inst->release & 0x07) : 0;

        // If attack is 0, jump to peak immediately (matches GBA PSG behavior)
        if (env_attack == 0) {
            env_level = 15;
            env_phase = (env_decay == 0) ? ENV_SUSTAIN : ENV_DECAY;
            int frames = (env_decay == 0) ? 1 : env_decay;
            frame_counter = g_samples_per_frame * frames;
        } else {
            env_phase = ENV_ATTACK;
            env_level = 0; // 0..15
            frame_counter = g_samples_per_frame * env_attack;
        }
    } else {
        // DirectSound envelope (0-255, additive/multiplicative)
        env_attack = inst ? static_cast<uint8_t>(inst->attack) : 255;
        env_decay = inst ? static_cast<uint8_t>(inst->decay) : 0;
        env_sustain = inst ? static_cast<uint8_t>(inst->sustain) : 255;
        env_release = inst ? static_cast<uint8_t>(inst->release) : 0;

        env_phase = ENV_ATTACK;
        env_level = (env_attack == 255) ? 255 : 0;
        frame_counter = g_samples_per_frame;
    }
}

void Voice::noteOff() {
    releasing = true;
    env_phase = ENV_RELEASE;
}

void Voice::updateEnvelope() {
    // Update once per frame (or PSG step length)
    if (--frame_counter > 0) {
        return;
    }

    if (is_psg) {
        auto reset_counter = [&](uint8_t env_frames) {
            int frames = (env_frames == 0) ? 1 : env_frames;
            frame_counter = g_samples_per_frame * frames;
        };

        switch (env_phase) {
        case ENV_ATTACK:
            if (env_level < 15) {
                env_level++;
            }
            if (env_level >= 15) {
                env_level = 15;
                env_phase = (env_decay == 0) ? ENV_SUSTAIN : ENV_DECAY;
            }
            reset_counter(env_attack);
            break;

        case ENV_DECAY:
            if (env_level > env_sustain) {
                env_level--;
            }
            if (env_level <= env_sustain) {
                env_level = env_sustain;
                env_phase = (env_level == 0) ? ENV_RELEASE : ENV_SUSTAIN;
            }
            reset_counter(env_decay);
            break;

        case ENV_SUSTAIN:
            // Hold at sustain level until note off
            reset_counter(1);
            break;

        case ENV_RELEASE:
            if (env_release == 0 || env_level == 0) {
                env_level = 0;
                is_active = false;
                break;
            }
            if (env_level > 0) {
                env_level--;
            }
            if (env_level == 0) {
                is_active = false;
            } else {
                reset_counter(env_release);
            }
            break;
        }
        return;
    }

    frame_counter = g_samples_per_frame;

    switch (env_phase) {
    case ENV_ATTACK:
        {
            // Attack is additive: level += attack
            uint32_t new_level = env_level + env_attack;
            if (new_level >= 255) {
                env_level = 255;
                env_phase = ENV_DECAY;
            } else {
                env_level = static_cast<uint8_t>(new_level);
            }
        }
        break;

    case ENV_DECAY:
        // Decay is multiplicative: level = (level * decay) >> 8
        // Higher decay value = slower decay (255 = almost no change)
        env_level = static_cast<uint8_t>((env_level * env_decay) >> 8);
        if (env_level <= env_sustain) {
            env_level = env_sustain;
            if (env_level == 0) {
                // If sustain is 0, go to release/die
                env_phase = ENV_RELEASE;
            } else {
                env_phase = ENV_SUSTAIN;
            }
        }
        break;

    case ENV_SUSTAIN:
        // Hold at sustain level until note off
        break;

    case ENV_RELEASE:
        // Release is multiplicative: level = (level * release) >> 8
        // Higher release value = slower release (255 = almost no change)
        env_level = static_cast<uint8_t>((env_level * env_release) >> 8);
        if (env_level == 0) {
            is_active = false;
        }
        break;
    }
}



void Mixer::setInstrumentData(const VoiceGroup *vg, const QMap<QString, VoiceGroup> *all_vg,
                              const QMap<QString, Sample> *samples,
                              const QMap<QString, QByteArray> *pcm_data,
                              const QMap<QString, KeysplitTable> *keysplit_tables) {
    m_voicegroup = vg;
    m_all_voicegroups = all_vg;
    m_samples = samples;
    m_pcm_data = pcm_data;
    m_keysplit_tables = keysplit_tables;

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

    // Handle voice_keysplit (0x40) - uses keysplit table to map MIDI key to instrument index
    if (inst->type_id == 0x40) {
        // sample_label contains the name of the referenced voicegroup
        auto vg_it = m_all_voicegroups->find(inst->sample_label);
        if (vg_it == m_all_voicegroups->end()) {
            qDebug() << "resolveInstrument: keysplit voicegroup not found:" << inst->sample_label;
            return nullptr;
        }

        // Look up the keysplit table to map MIDI key to instrument index
        int inst_index = 0;
        if (m_keysplit_tables && !inst->keysplit_table.isEmpty()) {
            auto table_it = m_keysplit_tables->find(inst->keysplit_table);
            if (table_it != m_keysplit_tables->end()) {
                const KeysplitTable &table = table_it.value();
                auto note_it = table.note_map.find(key);
                if (note_it != table.note_map.end()) {
                    inst_index = note_it.value();
                } else {
                    // Find the highest key <= requested key in the table
                    for (auto it = table.note_map.begin(); it != table.note_map.end(); ++it) {
                        if (it.key() <= key) {
                            inst_index = it.value();
                        }
                    }
                }
            } else {
                qDebug() << "resolveInstrument: keysplit table not found:" << inst->keysplit_table;
            }
        }

        const VoiceGroup &split_vg = vg_it.value();
        if (inst_index < 0 || inst_index >= split_vg.instruments.size()) {
            qDebug() << "resolveInstrument: key" << key << "inst_index" << inst_index
                     << "out of range for" << inst->sample_label
                     << "(size:" << split_vg.instruments.size() << ")";
            return nullptr;
        }

        if (out_vg) *out_vg = &split_vg;
        // Recursively resolve in case of nested keysplits
        return resolveInstrument(&split_vg.instruments[inst_index], key, out_vg);
    }

    // Handle voice_keysplit_all (0x80) - uses direct offset (drumsets)
    if (inst->type_id == 0x80) {
        // sample_label contains the name of the referenced voicegroup
        auto it = m_all_voicegroups->find(inst->sample_label);
        if (it == m_all_voicegroups->end()) {
            qDebug() << "resolveInstrument: keysplit_all voicegroup not found:" << inst->sample_label;
            return nullptr;
        }

        const VoiceGroup &drum_vg = it.value();
        int drum_index = key - drum_vg.offset;

        if (drum_index < 0 || drum_index >= drum_vg.instruments.size()) {
            qDebug() << "resolveInstrument: key" << key << "out of range for" << inst->sample_label
                     << "(offset:" << drum_vg.offset << "size:" << drum_vg.instruments.size() << ")";
            return nullptr;
        }

        if (out_vg) *out_vg = &drum_vg;
        // Recursively resolve in case of nested keysplits
        return resolveInstrument(&drum_vg.instruments[drum_index], key, out_vg);
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

    if (!m_voicegroup) {
        qDebug() << "noteOn: ch" << channel << "prog" << program << "- no voicegroup set";
        return;
    }

    if (program >= m_voicegroup->instruments.size()) {
        qDebug() << "noteOn: ch" << channel << "prog" << program
                 << "- program out of range (voicegroup has" << m_voicegroup->instruments.size() << "instruments)";
        return;
    }

    inst = &m_voicegroup->instruments[program];

    // Resolve keysplit/drumset instruments to actual instrument
    const VoiceGroup *resolved_vg = nullptr;
    inst = resolveInstrument(inst, key, &resolved_vg);

    if (!inst) {
        qDebug() << "noteOn: ch" << channel << "prog" << program << "key" << key
                 << "- failed to resolve keysplit (type_id:"
                 << m_voicegroup->instruments[program].type_id << ")";
        return;
    }

    // Look up sample/wave data based on voice type
    uint8_t type = inst->type_id;
    bool is_directsound = (type == 0x00 || type == 0x08 || type == 0x10);
    bool is_wave = (type == 0x03 || type == 0x0B);

    if (is_directsound && m_samples && !inst->sample_label.isEmpty()) {
        auto it = m_samples->find(inst->sample_label);
        if (it != m_samples->end()) {
            sample = &it.value();
            qDebug() << "noteOn: ch" << channel << "key" << key
                     << "sample:" << inst->sample_label
                     << "len:" << sample->data.size()
                     << "rate:" << sample->sample_rate
                     << "loop:" << sample->loops
                     << "loop_start:" << sample->loop_start
                     << "loop_end:" << sample->loop_end;
        } else {
            qDebug() << "noteOn: ch" << channel << "prog" << program
                     << "- DirectSound sample not found:" << inst->sample_label;
        }
    }

    if (is_wave && m_pcm_data && !inst->sample_label.isEmpty()) {
        auto it = m_pcm_data->find(inst->sample_label);
        if (it != m_pcm_data->end()) {
            wave_data = reinterpret_cast<const uint8_t *>(it.value().constData());
        } else {
            qDebug() << "noteOn: ch" << channel << "prog" << program
                     << "- ProgrammableWave data not found:" << inst->sample_label;
        }
    }

    // Store info for UI display
    if (channel < g_num_midi_channels) {
        m_channel_play_info[channel].active = true;
        m_channel_play_info[channel].voice_type = voiceTypeAbbrev(inst);
    }

    Voice *voice = nullptr;
    int voice_index = -1;
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].is_active &&
            m_voices[i].channel == channel &&
            m_voices[i].midi_key == key) {
            voice = &m_voices[i];
            voice_index = i;
            break;
        }
    }
    if (!voice) {
        voice = allocateVoice(key);
        voice_index = static_cast<int>(voice - m_voices);
    }
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

void Mixer::setChannelMute(uint8_t channel, bool muted) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].muted = muted;
    }
}

void Mixer::setAllMuted(bool muted) {
    for (int ch = 0; ch < g_num_midi_channels; ch++) {
        m_channels[ch].muted = muted;
    }
}

void Mixer::setMasterVolume(float volume) {
    float clamped = std::clamp(volume, 0.0f, 1.0f);
    m_master_volume.store(clamped, std::memory_order_relaxed);
}

float Mixer::masterVolume() const {
    return m_master_volume.load(std::memory_order_relaxed);
}

void Mixer::getMeterLevels(MeterLevels *out) const {
    if (!out) return;
    out->master_l = m_master_peak_l.load(std::memory_order_relaxed);
    out->master_r = m_master_peak_r.load(std::memory_order_relaxed);
    for (int i = 0; i < g_num_midi_channels; ++i) {
        out->channel_l[i] = m_channel_peak_l[i].load(std::memory_order_relaxed);
        out->channel_r[i] = m_channel_peak_r[i].load(std::memory_order_relaxed);
    }
}

void Mixer::processAudio(float *out_buffer, unsigned long frame_count) {
    float master_volume = m_master_volume.load(std::memory_order_relaxed);

    float master_peak_l = 0.0f;
    float master_peak_r = 0.0f;
    float channel_peak_l[g_num_midi_channels] = {0};
    float channel_peak_r[g_num_midi_channels] = {0};
    bool channel_active[g_num_midi_channels] = {0};

    for (unsigned long i = 0; i < frame_count; i++) {
        float mixed_l = 0.0f;
        float mixed_r = 0.0f;

        for (int v = 0; v < g_max_voices; v++) {
            if (m_voices[v].is_active) {
                uint8_t ch = m_voices[v].channel;
                channel_active[ch] = true;

                // Skip muted channels (but still advance the voice state)
                if (m_channels[ch].muted) {
                    m_voices[v].getNextSample(1.0f);  // advance state without output
                    continue;
                }

                // Calculate pitch bend multiplier (±2 semitones range)
                // pitch_bend is -8192 to +8191, map to ±2 semitones
                float bend_semitones = (m_channels[ch].pitch_bend / 8192.0f) * 2.0f;
                float pitch_multiplier = std::pow(2.0f, bend_semitones / 12.0f);

                float sample = m_voices[v].getNextSample(pitch_multiplier);

                // Apply channel volume and expression
                float ch_vol = m_channels[ch].volume / 127.0f;
                float ch_exp = m_channels[ch].expression / 127.0f;
                sample *= ch_vol * ch_exp;

                // MP2K/GBA-style pan math (linear, signed -128..128)
                int pan = static_cast<int>(m_channels[ch].pan) - 64; // 0..127 -> -64..63
                if (m_voices[v].inst_pan_enabled) {
                    pan += m_voices[v].inst_pan;
                }
                if (pan < -128) pan = -128;
                if (pan > 128) pan = 128;
                if (pan >= 126) pan = 128; // match mp2k edge clamp

                float left_gain = static_cast<float>(-pan + 128) * (1.0f / 256.0f);
                float right_gain = static_cast<float>(pan + 128) * (1.0f / 256.0f);

                float out_l = sample * left_gain;
                float out_r = sample * right_gain;

                mixed_l += out_l;
                mixed_r += out_r;

                float abs_l = std::fabs(out_l);
                float abs_r = std::fabs(out_r);
                if (abs_l > channel_peak_l[ch]) channel_peak_l[ch] = abs_l;
                if (abs_r > channel_peak_r[ch]) channel_peak_r[ch] = abs_r;
            }
        }

        mixed_l *= master_volume;
        mixed_r *= master_volume;

        float abs_ml = std::fabs(mixed_l);
        float abs_mr = std::fabs(mixed_r);
        if (abs_ml > master_peak_l) master_peak_l = abs_ml;
        if (abs_mr > master_peak_r) master_peak_r = abs_mr;

        // clip the volume so it doesnt blow out my speakers
        if (mixed_l > 1.0f) mixed_l = 1.0f;
        else if (mixed_l < -1.0f) mixed_l = -1.0f;

        if (mixed_r > 1.0f) mixed_r = 1.0f;
        else if (mixed_r < -1.0f) mixed_r = -1.0f;

        *out_buffer++ = mixed_l;
        *out_buffer++ = mixed_r;
    }

    m_master_peak_l.store(master_peak_l, std::memory_order_relaxed);
    m_master_peak_r.store(master_peak_r, std::memory_order_relaxed);
    for (int ch = 0; ch < g_num_midi_channels; ++ch) {
        m_channel_peak_l[ch].store(channel_peak_l[ch], std::memory_order_relaxed);
        m_channel_peak_r[ch].store(channel_peak_r[ch], std::memory_order_relaxed);
        m_channel_play_info[ch].active = channel_active[ch];
    }
}

void Mixer::end() {
    for (int i = 0; i < g_max_voices; i++) {
        m_voices[i].is_active = false;
        m_voices[i].releasing = false;
    }

    // Clear play info
    for (int i = 0; i < g_num_midi_channels; i++) {
        m_channel_play_info[i].active = false;
        m_channel_play_info[i].voice_type.clear();
    }

    m_master_peak_l.store(0.0f, std::memory_order_relaxed);
    m_master_peak_r.store(0.0f, std::memory_order_relaxed);
    for (int ch = 0; ch < g_num_midi_channels; ++ch) {
        m_channel_peak_l[ch].store(0.0f, std::memory_order_relaxed);
        m_channel_peak_r[ch].store(0.0f, std::memory_order_relaxed);
    }
}

Mixer::ChannelPlayInfo Mixer::getChannelPlayInfo(uint8_t channel) const {
    if (channel < g_num_midi_channels) {
        return m_channel_play_info[channel];
    }
    return ChannelPlayInfo();
}
