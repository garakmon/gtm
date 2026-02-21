#pragma once
#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

#include "util/constants.h"

#include <QByteArray>
#include <QMap>
#include <QString>

#include <cstdint>



enum class GbaVoiceType {
    DirectSound      = 0x00,
    Square1          = 0x01,
    Square2          = 0x02,
    ProgrammableWave = 0x03,
    Noise            = 0x04,
    Keysplit         = 0x40,
    KeysplitAll      = 0x80,
};



struct Sample {
    QByteArray data;           // 8-bit signed PCM
    uint32_t sample_rate = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;
    bool loops = false;
};



struct KeysplitTable {
    QString label;
    int offset = 0;
    // MIDI note: instrument map
    QMap<int, int> note_map;
};



struct Instrument {
    QString type; // eg. "voice_directsound"
    int type_id = 0;
    int base_key = g_midi_middle_c;
    int pan = 0;
    QString sample_label; // for instruments with samples in the map,
                          // or keysplit voicegroup names
    QString keysplit_table; // for voice_keysplit: the keysplit table name

    int attack = 0;
    int decay = 0;
    int sustain = 0x0f;
    int release = 0;

    int sweep = 0; // square 1 only
    int duty_cycle = 0; // square 1 & 2
};



struct VoiceGroup {
    int offset = 0; // starting MIDI note (for drumsets)
    QVector<Instrument> instruments;
};



// Limited version of the Song class, used to get basic info on songs
// without needing to fully load the midi file into memory
struct SongEntry {
    QString title;
    QString voicegroup;
    QString player;
    int type = 0;

    int volume = 0x7F;
    int priority = 0;
    int reverb = 0;

    QString midifile; // idk if necessary, should always be <title>.mid
};



struct ChannelContext {
    uint8_t program_id = 0;
    uint8_t volume = 100;
    uint8_t expression = 0x7F;
    uint8_t pan = 0x40;
    int16_t pitch_bend = 0;
    uint8_t priority = 0;
    bool muted = false;

    // LFO state (m4a modulation)
    uint8_t mod = 0;           // CC 1: modulation depth (0-127)
    uint8_t lfos = 22;         // CC 21: LFO speed (default 22)
    uint8_t lfodl = 0;         // CC 26: LFO delay (ticks)
    uint8_t lfodl_count = 0;   // current delay countdown
    uint8_t lfo_phase = 0;     // triangle wave phase (wrapping uint8_t)
    int8_t lfo_value = 0;      // current LFO output
    uint8_t modt = 0;          // CC 22: 0=pitch, 1=volume, 2=pan
};



struct SoundChannel {
    bool is_active = false;
    uint8_t current_midi_key = 0;
    ChannelContext *context = nullptr;

    // Rendering State
    const int16_t *sample_buffer = nullptr;
    uint32_t sample_length = 0x00;
    double current_cursor = 0.0;
    double step_size = 0.0; // calculated frequency ratio

    // ADSR State
    uint8_t adsr_phase = 0;
    float envelope_volume = 0.0f;
};



struct Voice {
    bool is_active = false;
    uint8_t channel = 0;
    uint8_t midi_key = 0;
    uint8_t velocity = 0;
    bool releasing = false;

    // Voice type (from GbaVoiceType enum)
    uint8_t voice_type = 0;
    int8_t inst_pan = 0;       // instrument pan offset (signed), if enabled
    bool inst_pan_enabled = false;
    bool is_fixed = false;     // type 0x08: play at native rate, ignore key/pitch bend

    // Sample playback (DirectSound)
    const int8_t *sample_data = nullptr;
    uint32_t sample_length = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;
    bool loops = false;
    double position = 0.0;
    double pitch_ratio = 1.0;

    // Square wave / Programmable wave synthesis
    uint8_t duty_cycle = 2;  // 0=12.5%, 1=25%, 2=50%, 3=75%
    double phase = 0.0;
    double phase_inc = 0.0;  // phase increment per sample

    // Programmable wave (16 4-bit samples stored in 32 bytes)
    const uint8_t *wave_data = nullptr;

    // Noise generation (LFSR)
    uint16_t lfsr = 0x7FFF;
    uint16_t noise_mask = 0x6000; // Galois LFSR mask (15-bit default)
    int noise_counter = 0;
    int noise_period = 1;

    // ADSR Envelope (GBA-accurate: frame-based, 0-255 level)
    enum EnvPhase { ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
    EnvPhase env_phase = ENV_ATTACK;
    uint8_t env_level = 0;       // current envelope level (0-255, GBA-style)
    uint8_t env_peak = 15;      // PSG: peak level from velocity/volume (0-15)
    uint8_t env_attack = 0;      // attack value (higher = faster, additive per frame)
    uint8_t env_decay = 0;       // decay value (higher = slower, multiplicative)
    uint8_t env_sustain = 0;     // sustain level (0-255)
    uint8_t env_release = 0;     // release value (higher = slower, multiplicative)
    uint8_t psg_sus_raw = 15;   // PSG: raw instrument sustain param (0-15)
    int frame_counter = 0;       // samples until next envelope update
    bool is_psg = false;         // square/wave/noise use GBA PSG-style envelopes

    float getNextSample(float pitch_bend_multiplier = 1.0f);
    void noteOn(uint8_t ch, uint8_t key, uint8_t vel, const Instrument *inst,
                const Sample *sample, const uint8_t *wave);
    void noteOff();
    void updateEnvelope();
};

#endif // SOUNDTYPES_H
