#include "sound/mixer.h"

#include <cmath>
#include <algorithm>
#include <QDebug>



// duty cycle thresholds for square waves (12.5%, 25%, 50%, 75%)
static const double k_duty_thresholds[4] = {0.125, 0.25, 0.5, 0.75};

/**
 *  Helper to get voice type abbreviation (based on agbplay)
 */
static QString voiceTypeAbbrev(const Instrument *inst) {
    if (!inst) return "-";

    static const QMap<int, QString> s_base_type = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> s_duty_map = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    const int type_id = inst->type_id;
    if (s_base_type.contains(type_id)) {
        return s_base_type.value(type_id);
    }

    const QString duty = s_duty_map.value(inst->duty_cycle & 0x03, "50");

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

/**
 * Generates the next raw sample for the voice’s current instrument type, advances its
 * playback position or oscillator phase, updates the envelope,
 * and returns the final per-voice sample amplitude.
 */
float Voice::getNextSample(float pitch_bend_multiplier) {
    if (!this->is_active) return 0.0f;

    float sample = 0.0f;

    switch (this->voice_type) {
    case 0x00: // DirectSound
    case 0x08: // DirectSound (no resample)
    case 0x10: // DirectSound (alt)
        if (this->sample_data && this->sample_length > 0) {
            // use point sampling with no interpolation
            int idx = static_cast<int>(this->position);

            if (this->loops && this->loop_end > this->loop_start) {
                // looping sample: wrap position within loop region
                if (idx >= static_cast<int>(this->loop_end)) {
                    this->position = this->loop_start +
                                     std::fmod(this->position - this->loop_start,
                                               this->loop_end - this->loop_start);
                    idx = static_cast<int>(this->position);
                }
            } else {
                // non-looping sample: check for end
                if (idx >= static_cast<int>(this->sample_length)) {
                    this->is_active = false;
                    return 0.0f;
                }
            }

            // convert signed 8-bit pcm to float amplitude
            sample = this->sample_data[idx] / 128.0f;

            // fixed-rate voices ignore pitch bend
            this->position += this->is_fixed ?
                              this->pitch_ratio :
                              (this->pitch_ratio * pitch_bend_multiplier);
        }
        break;
    case 0x01: // Square1
    case 0x09: // Square1 (alt)
    case 0x02: // Square2
    case 0x0A: // Square2 (alt)
        {
            double threshold = k_duty_thresholds[this->duty_cycle & 0x03];
            sample = (this->phase < threshold) ? 0.5f : -0.5f;

            this->phase += this->phase_inc * pitch_bend_multiplier;
            if (this->phase >= 1.0) {
                this->phase -= 1.0;
            }
        }
        break;
    case 0x03: // ProgrammableWave
    case 0x0B: // ProgrammableWave (alt)
        if (this->wave_data) {
            // 32 bytes = 16 4-bit samples, each byte has 2 samples
            int wave_idx = static_cast<int>(this->phase * 32.0) % 32;
            uint8_t byte = this->wave_data[wave_idx / 2];
            // high nibble first, then low nibble
            uint8_t nibble = (wave_idx & 1) ? (byte & 0x0F) : (byte >> 4);
            // convert 0-15 to -1.0 to 1.0
            sample = (nibble / 7.5f) - 1.0f;

            this->phase += this->phase_inc * pitch_bend_multiplier;
            if (this->phase >= 1.0) this->phase -= 1.0;
        }
        break;
    case 0x04: // Noise
    case 0x0C: // Noise (alt)
        {
            // clock the LFSR based on noise_period (Galois form)
            if (++this->noise_counter >= this->noise_period) {
                this->noise_counter = 0;
                if (this->lfsr & 1) {
                    this->lfsr >>= 1;
                    this->lfsr ^= this->noise_mask;
                } else {
                    this->lfsr >>= 1;
                }
            }
            sample = (this->lfsr & 1) ? 0.5f : -0.5f;
        }
        break;

    default:
        break;
    }

    // process ADSR envelope
    this->updateEnvelope();

    if (!this->is_active) return 0.0f;

    // convert envelope level to float
    if (this->is_psg) {
        // PSG: velocity/volume are baked into env_peak, don't apply separately
        return sample * (this->env_level / 15.0f);
    } else {
        return sample * (this->env_level / 255.0f) * (this->velocity / 127.0f);
    }
}

/**
 * The per-note voice initializer.
 * Initialize the voice for a new note by setting channel + key + velocity, configuring
 * the correct waveform or sample source, computing pitch behavior, and setting up the
 * appropriate ADSR envelope state. 
 * Ensure future calls to getNextSample() generate the correct sound,
 * 
 * !TODO: need to finish this (PSG peak for example)
 */
void Voice::noteOn(uint8_t ch, uint8_t key, uint8_t vel, const Instrument *inst,
                   const Sample *sample, const uint8_t *wave) {
    this->channel = ch;
    this->midi_key = key;
    this->velocity = vel;
    this->is_active = true;
    this->releasing = false;
    this->position = 0.0;
    this->phase = 0.0;
    this->is_psg = false;
    this->is_fixed = false;

    int base_key = inst ? inst->base_key : g_midi_middle_c;
    this->voice_type = inst ? inst->type_id : 0;
    this->inst_pan_enabled = (inst && inst->pan != 0);
    this->inst_pan = this->inst_pan_enabled
                     ? static_cast<int8_t>(std::clamp(inst->pan, -128, 127))
                     : 0;

    // MIDI note to frequency: f = 440 * 2^((note - 69) / 12)
    double frequency = 440.0 * std::pow(2.0, (key - 69) / 12.0);
    this->phase_inc = frequency / g_sample_rate;

    switch (this->voice_type) {
    case 0x00: // DirectSound
    case 0x08: // DirectSound (fixed rate - no pitch adjustment)
    case 0x10: // DirectSound (alt)
        if (sample && !sample->data.isEmpty()) {
            this->sample_data = reinterpret_cast<const int8_t *>(sample->data.constData());
            this->sample_length = sample->data.size();
            this->loop_start = sample->loop_start;
            this->loop_end = sample->loop_end > 0 ? sample->loop_end : this->sample_length;
            this->loops = sample->loops;

            if (this->voice_type == 0x08) {
                // fixed-rate: play at native sample rate, ignore MIDI key
                this->is_fixed = true;
                this->pitch_ratio = static_cast<double>(sample->sample_rate) / g_sample_rate;
            } else {
                double freq_ratio = std::pow(2.0, (key - base_key) / 12.0);
                this->pitch_ratio =
                    (static_cast<double>(sample->sample_rate) / g_sample_rate) * freq_ratio;
            }
        } else {
            this->sample_data = nullptr;
            this->sample_length = 0;
            this->pitch_ratio = 1.0;
        }
        break;

    case 0x01: // Square1
    case 0x09: // Square1 (alt)
    case 0x02: // Square2
    case 0x0A: // Square2 (alt)
        this->is_psg = true;
        this->duty_cycle = inst ? inst->duty_cycle : 2;
        break;

    case 0x03: // ProgrammableWave
    case 0x0B: // ProgrammableWave (alt)
        this->is_psg = true;
        this->wave_data = wave;
        break;

    case 0x04: // Noise
    case 0x0C: // Noise (alt)
        this->is_psg = true;
        {
            const int instr_np = inst ? inst->duty_cycle : 0; // noise period/mode param
            if (instr_np & 0x1) {
                // 7-bit LFSR
                this->lfsr = 0x40;
                this->noise_mask = 0x60;
            } else {
                // 15-bit LFSR
                this->lfsr = 0x4000;
                this->noise_mask = 0x6000;
            }
        }
        this->noise_counter = 0;
        // approximate GBA noise pitch behavior
        // (credit ipatix/agbplay -- MP2KChnPSGNoise::SetPitch)
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
            this->noise_period =
                std::max(1, static_cast<int>(std::round(g_sample_rate / noisefreq)));
        }
        break;

    default:
        break;
    }

    // set up ADSR envelope
    if (this->is_psg) {
        // PSG envelopes use small integer steps (0-7 / 0-15)
        this->env_attack = inst ? static_cast<uint8_t>(inst->attack & 0x07) : 0;
        this->env_decay = inst ? static_cast<uint8_t>(inst->decay & 0x07) : 0;
        this->psg_sus_raw = inst ? static_cast<uint8_t>(inst->sustain & 0x0F) : 15;
        this->env_release = inst ? static_cast<uint8_t>(inst->release & 0x07) : 0;

        // defaults: env_peak and env_sustain get set by updatePsgVoicePeak() after noteOn
        this->env_peak = 15;
        this->env_sustain = this->psg_sus_raw;

        // if attack is 0, jump to peak immediately
        if (this->env_attack == 0) {
            this->env_level = this->env_peak;
            this->env_phase = (this->env_decay == 0) ? ENV_SUSTAIN : ENV_DECAY;
            int frames = (this->env_decay == 0) ? 1 : this->env_decay;
            this->frame_counter = g_samples_per_frame * frames;
        } else {
            this->env_phase = ENV_ATTACK;
            this->env_level = 0; // 0-15
            this->frame_counter = g_samples_per_frame * this->env_attack;
        }
    } else {
        // DirectSound envelope (0-255, additive/multiplicative)
        this->env_attack = inst ? static_cast<uint8_t>(inst->attack) : 0xff;
        this->env_decay = inst ? static_cast<uint8_t>(inst->decay) : 0;
        this->env_sustain = inst ? static_cast<uint8_t>(inst->sustain) : 0xff;
        this->env_release = inst ? static_cast<uint8_t>(inst->release) : 0;

        this->env_phase = ENV_ATTACK;
        this->env_level = (this->env_attack == 0xff) ? 0xff : 0;
        this->frame_counter = g_samples_per_frame;
    }
}

/**
 * Mark the voice as releasing.
 */
void Voice::noteOff() {
    this->releasing = true;
    this->env_phase = ENV_RELEASE;
}

/**
 * Advance the voice’s ADSR envelope based on its current phase and instrument type.
 * Update envelope level and deactivate the voice when the release reaches zero.
 */
void Voice::updateEnvelope() {
    // update once per frame
    if (--this->frame_counter > 0) {
        return;
    }

    if (this->is_psg) {
        auto reset_counter = [&](uint8_t env_frames) {
            int frames = (env_frames == 0) ? 1 : env_frames;
            this->frame_counter = g_samples_per_frame * frames;
        };

        switch (this->env_phase) {
        case ENV_ATTACK:
            if (this->env_level < this->env_peak) {
                this->env_level++;
            }
            if (this->env_level >= this->env_peak) {
                this->env_level = this->env_peak;
                this->env_phase = (this->env_decay == 0) ? ENV_SUSTAIN : ENV_DECAY;
            }
            reset_counter(this->env_attack);
            break;

        case ENV_DECAY:
            if (this->env_level > this->env_sustain) {
                this->env_level--;
            }
            if (this->env_level <= this->env_sustain) {
                this->env_level = this->env_sustain;
                this->env_phase = (this->env_level == 0) ? ENV_RELEASE : ENV_SUSTAIN;
            }
            reset_counter(this->env_decay);
            break;

        case ENV_SUSTAIN:
            // hold at sustain level until note off
            reset_counter(1);
            break;

        case ENV_RELEASE:
            if (this->env_release == 0 || this->env_level == 0) {
                this->env_level = 0;
                this->is_active = false;
                break;
            }
            if (this->env_level > 0) {
                this->env_level--;
            }
            if (this->env_level == 0) {
                this->is_active = false;
            } else {
                reset_counter(this->env_release);
            }
            break;
        }
        return;
    }

    this->frame_counter = g_samples_per_frame;

    switch (this->env_phase) {
    case ENV_ATTACK:
        {
            // attack is additive: level += attack
            uint32_t new_level = this->env_level + this->env_attack;
            if (new_level >= 0xff) {
                this->env_level = 0xff;
                this->env_phase = ENV_DECAY;
            } else {
                this->env_level = static_cast<uint8_t>(new_level);
            }
        }
        break;

    case ENV_DECAY:
        // decay is multiplicative: level = (level * decay) >> 8
        // higher decay value = slower decay (0xff = almost no change)
        this->env_level = static_cast<uint8_t>((this->env_level * this->env_decay) >> 8);
        if (this->env_level <= this->env_sustain) {
            this->env_level = this->env_sustain;
            if (this->env_level == 0) {
                // if sustain is 0, go to release/die
                this->env_phase = ENV_RELEASE;
            } else {
                this->env_phase = ENV_SUSTAIN;
            }
        }
        break;

    case ENV_SUSTAIN:
        // hold at sustain level until note off
        break;

    case ENV_RELEASE:
        // release is multiplicative: level = (level * release) >> 8
        // higher release value = slower release (0xff = almost no change)
        this->env_level = static_cast<uint8_t>((this->env_level * this->env_release) >> 8);
        if (this->env_level == 0) {
            this->is_active = false;
        }
        break;
    }
}



Mixer::Mixer() {
    for (int i = 0; i < g_num_midi_channels; ++i) {
        m_channel_peak_l[i].store(0.0f, std::memory_order_relaxed);
        m_channel_peak_r[i].store(0.0f, std::memory_order_relaxed);
    }
}

/**
 * Render the next block of stereo audio by advancing active voices,
 * applying channel controls/effects, mixing them, and writing the final samples.
 */
void Mixer::processAudio(float *out_buffer, unsigned long frame_count) {
    float master_volume = m_master_volume.load(std::memory_order_relaxed);
    float song_volume = m_song_volume.load(std::memory_order_relaxed);

    float master_peak_l = 0.0f;
    float master_peak_r = 0.0f;
    float channel_peak_l[g_num_midi_channels] = {0};
    float channel_peak_r[g_num_midi_channels] = {0};
    bool channel_active[g_num_midi_channels] = {0};

    for (unsigned long i = 0; i < frame_count; i++) {
        // tick the per-channel lfos at the engine tick rate
        if (++m_lfo_counter >= m_samples_per_lfo_tick) {
            m_lfo_counter = 0;
            for (int ch = 0; ch < g_num_midi_channels; ch++) {
                this->tickLfo(ch);
            }
        }

        float pcm_l = 0.0f, pcm_r = 0.0f;
        float psg_l = 0.0f, psg_r = 0.0f;

        for (int v = 0; v < g_max_voices; v++) {
            if (m_voices[v].is_active) {
                uint8_t ch = m_voices[v].channel;
                channel_active[ch] = true;

                // muted channels still advance so timing stays consistent
                if (m_channels[ch].muted) {
                    m_voices[v].getNextSample(1.0f);  // advance state without output
                    continue;
                }

                // calculate the full pitch multiplier before sampling
                float bend_semitones = (m_channels[ch].pitch_bend / 8192.0f) * 2.0f;
                float pitch_multiplier = std::pow(2.0f, bend_semitones / 12.0f);

                // apply pitch lfo
                if (m_channels[ch].modt == 0 && m_channels[ch].lfo_value != 0) {
                    // lfo_value * 4 in pitch units (1/64 semitone)
                    pitch_multiplier *= std::pow(2.0f,
                        m_channels[ch].lfo_value * 4.0f / 768.0f);
                }

                float sample = m_voices[v].getNextSample(pitch_multiplier);

                // directsound applies channel volume and expression here
                if (!m_voices[v].is_psg) {
                    float ch_vol = m_channels[ch].volume / 127.0f;
                    float ch_exp = m_channels[ch].expression / 127.0f;
                    sample *= ch_vol * ch_exp;
                }

                // apply volume lfo
                if (m_channels[ch].modt == 1 && m_channels[ch].lfo_value != 0) {
                    int lfo_vol_scale = m_channels[ch].lfo_value + 128;
                    if (lfo_vol_scale < 0) lfo_vol_scale = 0;
                    sample = sample * lfo_vol_scale / 128.0f;
                }

                // build the final pan value from track, instrument, and lfo
                int pan = static_cast<int>(m_channels[ch].pan) - 64; // 0..127 -> -64..63
                if (m_voices[v].inst_pan_enabled) {
                    pan += m_voices[v].inst_pan;
                }

                // apply pan lfo
                if (m_channels[ch].modt == 2) {
                    pan += m_channels[ch].lfo_value;
                }

                if (pan < -128) pan = -128;
                if (pan > 128) pan = 128;
                if (pan >= 126) pan = 128; // match mp2k edge clamp

                float left_gain = static_cast<float>(-pan + 128) * (1.0f / 256.0f);
                float right_gain = static_cast<float>(pan + 128) * (1.0f / 256.0f);

                float out_l = sample * left_gain;
                float out_r = sample * right_gain;

                // only pcm feeds reverb
                if (m_voices[v].is_psg) {
                    psg_l += out_l;
                    psg_r += out_r;
                } else {
                    pcm_l += out_l;
                    pcm_r += out_r;
                }

                float abs_l = std::fabs(out_l);
                float abs_r = std::fabs(out_r);
                if (abs_l > channel_peak_l[ch]) channel_peak_l[ch] = abs_l;
                if (abs_r > channel_peak_r[ch]) channel_peak_r[ch] = abs_r;
            }
        }

        // apply reverb to pcm only
        if (m_reverb_intensity > 0.0f) {
            float rev = (m_reverb_buf[m_reverb_pos].left + m_reverb_buf[m_reverb_pos].right
                       + m_reverb_buf[m_reverb_pos2].left + m_reverb_buf[m_reverb_pos2].right)
                       * m_reverb_intensity * 0.25f;
            pcm_l += rev;
            pcm_r += rev;
            m_reverb_buf[m_reverb_pos] = {pcm_l, pcm_r};
            if (++m_reverb_pos >= k_reverb_buf_size) m_reverb_pos = 0;
            if (++m_reverb_pos2 >= k_reverb_buf_size) m_reverb_pos2 = 0;
        }

        float mixed_l = (pcm_l + psg_l) * song_volume * master_volume;
        float mixed_r = (pcm_r + psg_r) * song_volume * master_volume;

        float abs_ml = std::fabs(mixed_l);
        float abs_mr = std::fabs(mixed_r);
        if (abs_ml > master_peak_l) master_peak_l = abs_ml;
        if (abs_mr > master_peak_r) master_peak_r = abs_mr;

        // clamp before writing to the output buffer
        if (mixed_l > 1.0f) mixed_l = 1.0f;
        else if (mixed_l < -1.0f) mixed_l = -1.0f;

        if (mixed_r > 1.0f) mixed_r = 1.0f;
        else if (mixed_r < -1.0f) mixed_r = -1.0f;

        *out_buffer++ = mixed_l;
        *out_buffer++ = mixed_r;
    }

    // update the live meter + state snapshot for the ui
    m_master_peak_l.store(master_peak_l, std::memory_order_relaxed);
    m_master_peak_r.store(master_peak_r, std::memory_order_relaxed);
    for (int ch = 0; ch < g_num_midi_channels; ++ch) {
        m_channel_peak_l[ch].store(channel_peak_l[ch], std::memory_order_relaxed);
        m_channel_peak_r[ch].store(channel_peak_r[ch], std::memory_order_relaxed);
        m_channel_play_info[ch].active = channel_active[ch];
    }
}

/**
 * Set the active voicegroup + sample data sources,
 * and reset per-channel runtime state to default values.
 */
void Mixer::setInstrumentData(const VoiceGroup *vg,
                              const QMap<QString, VoiceGroup> *all_vg,
                              const QMap<QString, Sample> *samples,
                              const QMap<QString, QByteArray> *pcm_data,
                              const QMap<QString, KeysplitTable> *keysplits) {
    m_voicegroup = vg;
    m_all_voicegroups = all_vg;
    m_samples = samples;
    m_pcm_data = pcm_data;
    m_keysplit_tables = keysplits;

    // reset channel state to defaults
    for (int i = 0; i < g_num_midi_channels; i++) {
        m_channels[i].volume = 100;     // default midi volume
        m_channels[i].expression = 127; // default expression
        m_channels[i].pan = 64;         // center pan
        m_channels[i].pitch_bend = 0;   // no bend
        // reset lfo state
        m_channels[i].mod = 0;
        m_channels[i].lfos = 22;
        m_channels[i].lfodl = 0;
        m_channels[i].lfodl_count = 0;
        m_channels[i].lfo_phase = 0;
        m_channels[i].lfo_value = 0;
        m_channels[i].modt = 0;
    }
    m_lfo_counter = 0;

    // reset reverb buffer
    m_reverb_buf.fill({});
    m_reverb_pos = 0;
    m_reverb_pos2 = k_reverb_buf_size / 2;
}

/**
 * Clear all active voices, ui play info, reverb state, and meter peaks.
 */
void Mixer::end() {
    for (int i = 0; i < g_max_voices; i++) {
        m_voices[i].is_active = false;
        m_voices[i].releasing = false;
    }

    // clear play info
    for (int i = 0; i < g_num_midi_channels; i++) {
        m_channel_play_info[i].active = false;
        m_channel_play_info[i].voice_type.clear();
    }

    // clear reverb buffer
    m_reverb_buf.fill({});
    m_reverb_pos = 0;
    m_reverb_pos2 = k_reverb_buf_size / 2;

    m_master_peak_l.store(0.0f, std::memory_order_relaxed);
    m_master_peak_r.store(0.0f, std::memory_order_relaxed);
    for (int ch = 0; ch < g_num_midi_channels; ++ch) {
        m_channel_peak_l[ch].store(0.0f, std::memory_order_relaxed);
        m_channel_peak_r[ch].store(0.0f, std::memory_order_relaxed);
    }
}

/**
 * Resolve the requested key to a concrete instrument, allocate or reuse a voice,
 * and start that note playing.
 */
void Mixer::noteOn(uint8_t channel, uint8_t key, uint8_t velocity, uint8_t program) {
    const Instrument *inst = nullptr;
    const Sample *sample = nullptr;
    const uint8_t *wave_data = nullptr;

    if (!m_voicegroup) {
        return;
    }

    if (program >= m_voicegroup->instruments.size()) {
        return;
    }

    inst = &m_voicegroup->instruments[program];
    const Instrument *base_inst = inst;

    // resolve keysplit and drumset wrappers
    const VoiceGroup *resolved_vg = nullptr;
    inst = this->resolveInstrument(inst, key, &resolved_vg);

    if (!inst) {
        return;
    }

    // find the concrete sample or wave payload
    uint8_t type = inst->type_id;
    bool is_directsound = (type == 0x00 || type == 0x08 || type == 0x10);
    bool is_wave = (type == 0x03 || type == 0x0B);

    if (is_directsound && m_samples && !inst->sample_label.isEmpty()) {
        auto it = m_samples->find(inst->sample_label);
        if (it != m_samples->end()) {
            sample = &it.value();
        }
    }

    if (is_wave && m_pcm_data && !inst->sample_label.isEmpty()) {
        auto it = m_pcm_data->find(inst->sample_label);
        if (it != m_pcm_data->end()) {
            wave_data = reinterpret_cast<const uint8_t *>(it.value().constData());
        }
    }

    // store info for UI display
    if (channel < g_num_midi_channels) {
        m_channel_play_info[channel].active = true;
        m_channel_play_info[channel].voice_type = voiceTypeAbbrev(inst);
    }

    Voice *voice = nullptr;
    // reuse an existing matching note first
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].is_active &&
            m_voices[i].channel == channel &&
            m_voices[i].midi_key == key) {
            voice = &m_voices[i];
            break;
        }
    }
    if (!voice) {
        voice = this->allocateVoice(key);
    }
    voice->noteOn(channel, key, velocity, inst, sample, wave_data);

    // fold current channel controls into the psg envelope peak
    if (voice->is_psg && channel < g_num_midi_channels) {
        this->updatePsgVoicePeak(voice, channel);
        // re-apply the peak when attack jumps immediately
        if (voice->env_attack == 0) {
            voice->env_level = voice->env_peak;
        }
    }
}

/**
 * Find the matching active note on the channel and puts its voice into release.
 */
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
        // update psg envelope peaks because volume is baked into them
        for (int i = 0; i < g_max_voices; i++) {
            if (m_voices[i].is_active
             && m_voices[i].is_psg
             && m_voices[i].channel == channel)
                this->updatePsgVoicePeak(&m_voices[i], channel);
        }
    }
}

void Mixer::setChannelPan(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].pan = value;
        for (int i = 0; i < g_max_voices; i++) {
            if (m_voices[i].is_active
             && m_voices[i].is_psg
             && m_voices[i].channel == channel)
                this->updatePsgVoicePeak(&m_voices[i], channel);
        }
    }
}

void Mixer::setChannelExpression(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].expression = value;
        for (int i = 0; i < g_max_voices; i++) {
            if (m_voices[i].is_active
             && m_voices[i].is_psg
             && m_voices[i].channel == channel)
                this->updatePsgVoicePeak(&m_voices[i], channel);
        }
    }
}

void Mixer::setChannelPitchBend(uint8_t channel, int value) {
    if (channel < g_num_midi_channels) {
        // store as signed offset from center
        m_channels[channel].pitch_bend = static_cast<int16_t>(value - 8192);
    }
}

void Mixer::setChannelMod(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].mod = value;
        if (value == 0) this->resetLfo(channel);
    }
}

void Mixer::setChannelLfos(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].lfos = value;
        if (value == 0) this->resetLfo(channel);
    }
}

void Mixer::setChannelModt(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].modt = value;
    }
}

void Mixer::setChannelLfodl(uint8_t channel, uint8_t value) {
    if (channel < g_num_midi_channels) {
        m_channels[channel].lfodl = value;
        m_channels[channel].lfodl_count = value;
    }
}

void Mixer::resetLfo(uint8_t channel) {
    if (channel < g_num_midi_channels) {
        // on note-on or when modulation is disabled, restart lfo state
        m_channels[channel].lfodl_count = m_channels[channel].lfodl;
        m_channels[channel].lfo_phase = 0;
        m_channels[channel].lfo_value = 0;
    }
}

void Mixer::setLfoTickRate(int samples_per_tick) {
    if (samples_per_tick > 0) {
        m_samples_per_lfo_tick = samples_per_tick;
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

void Mixer::setSongVolume(uint8_t vol) {
    // song volume from midi.cfg is 0-127, applied as a linear scaler
    float v = static_cast<float>(vol) / 128.0f;
    m_song_volume.store(v, std::memory_order_relaxed);
}

void Mixer::setReverbLevel(uint8_t level) {
    m_reverb_intensity = static_cast<float>(level) / 128.0f;
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

Mixer::ChannelPlayInfo Mixer::getChannelPlayInfo(uint8_t channel) const {
    if (channel < g_num_midi_channels) {
        return m_channel_play_info[channel];
    }
    return ChannelPlayInfo();
}

/**
 * Resolve keysplit and drumset wrapper instruments into real instruments for a given key.
 */
const Instrument *Mixer::resolveInstrument(const Instrument *inst, uint8_t key,
                                           const VoiceGroup **out_vg) const {
    if (!inst || !m_all_voicegroups) return inst;

    // resolve voice_keysplit through the selected table
    if (inst->type_id == 0x40) {
        // sample_label names the referenced voicegroup
        auto vg_it = m_all_voicegroups->find(inst->sample_label);
        if (vg_it == m_all_voicegroups->end()) {
            qDebug() << "resolveInstrument: keysplit voicegroup not found:"
                     << inst->sample_label;
            return nullptr;
        }

        // map the midi key to a concrete instrument index
        int inst_index = 0;
        if (m_keysplit_tables && !inst->keysplit_table.isEmpty()) {
            auto table_it = m_keysplit_tables->find(inst->keysplit_table);
            if (table_it != m_keysplit_tables->end()) {
                const KeysplitTable &table = table_it.value();
                auto note_it = table.note_map.find(key);
                if (note_it != table.note_map.end()) {
                    inst_index = note_it.value();
                } else {
                    // fall back to the highest key not above the request
                    for (auto it = table.note_map.begin();
                         it != table.note_map.end(); it++) {
                        if (it.key() <= key) {
                            inst_index = it.value();
                        }
                    }
                }
            } else {
                qDebug() << "resolveInstrument: keysplit table not found:"
                         << inst->keysplit_table;
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
        // recurse so nested keysplits still resolve correctly
        return this->resolveInstrument(&split_vg.instruments[inst_index], key, out_vg);
    }

    // resolve voice_keysplit_all by direct drum index
    if (inst->type_id == 0x80) {
        // sample_label names the referenced voicegroup
        auto it = m_all_voicegroups->find(inst->sample_label);
        if (it == m_all_voicegroups->end()) {
            qDebug() << "resolveInstrument: keysplit_all voicegroup not found:"
                     << inst->sample_label;
            return nullptr;
        }

        const VoiceGroup &drum_vg = it.value();
        int drum_index = key - drum_vg.offset;

        if (drum_index < 0 || drum_index >= drum_vg.instruments.size()) {
            qDebug() << "resolveInstrument: key" << key
                     << "out of range for" << inst->sample_label
                     << "(offset:" << drum_vg.offset
                     << "size:" << drum_vg.instruments.size() << ")";
            return nullptr;
        }

        if (out_vg) *out_vg = &drum_vg;
        // recurse so nested keysplits still resolve correctly
        return this->resolveInstrument(&drum_vg.instruments[drum_index], key, out_vg);
    }

    return inst;
}

/**
 * Choose a voice slot for a new note.
 * 
 * !TODO: use the right voice-stealing algorithm: pokeemerald/src/m4a_1.s (ply_note_)
 *        (free > stopped > priority > pointer value)
 */
Voice *Mixer::allocateVoice(uint8_t key) {
    // prefer an unused voice first
    for (int i = 0; i < g_max_voices; i++) {
        if (!m_voices[i].is_active) {
            return &m_voices[i];
        }
    }

    // then steal a voice that is already releasing
    for (int i = 0; i < g_max_voices; i++) {
        if (m_voices[i].releasing) {
            return &m_voices[i];
        }
    }

    // final fallback until real voice stealing is implemented
    return &m_voices[0];
}

/**
 * Recompute a PSG voice’s envelope peak and sustain level from current channel state.
 */
void Mixer::updatePsgVoicePeak(Voice *v, uint8_t ch) {
    // match agbplay's psg volume folding
    int vol = m_channels[ch].volume << 1;       // 0-254
    int exp = m_channels[ch].expression;
    vol = (vol * exp) >> 7;                      // fold in expression
    int pan = (static_cast<int>(m_channels[ch].pan) - 64) << 1;  // -128..126

    int rhythmPan = v->inst_pan_enabled ? static_cast<int>(v->inst_pan) : 0;

    int trkVolML = ((127 - pan) * vol) >> 8;
    int trkVolMR = ((pan + 128) * vol) >> 8;
    int chnVolL = (((127 - rhythmPan) * v->velocity) * trkVolML) >> 14;
    int chnVolR = (((rhythmPan + 128) * v->velocity) * trkVolMR) >> 14;

    uint8_t new_peak = static_cast<uint8_t>(std::clamp((chnVolL + chnVolR) >> 4, 0, 15));
    uint8_t new_sustain = static_cast<uint8_t>(
        std::clamp((new_peak * v->psg_sus_raw + 15) >> 4, 0, 15)
    );

    v->env_peak = new_peak;
    v->env_sustain = new_sustain;

    if (v->env_phase == Voice::ENV_SUSTAIN) {
        v->env_level = new_sustain;
    }
    if (v->env_level > new_peak && v->env_phase == Voice::ENV_ATTACK) {
        v->env_level = new_peak;
    }
}

/**
 * Advance one channel’s LFO state by one mixer LFO tick and updates its modulation.
 */
void Mixer::tickLfo(uint8_t ch) {
    auto &c = m_channels[ch];
    if (c.lfos == 0 || c.mod == 0) return;
    if (c.lfodl_count > 0) {
        c.lfodl_count--;
        return;
    }

    c.lfo_phase += c.lfos;
    int lfo_point;
    if (static_cast<int8_t>(c.lfo_phase - 64) >= 0)
        lfo_point = 128 - c.lfo_phase;
    else
        lfo_point = static_cast<int8_t>(c.lfo_phase);
    lfo_point = (lfo_point * c.mod) >> 6;
    c.lfo_value = static_cast<int8_t>(lfo_point);
}
