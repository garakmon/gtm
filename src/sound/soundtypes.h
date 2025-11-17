#pragma once
#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

#include <cstdint>
#include <QString>
#include <QByteArray>
#include <QMap>

#include "constants.h"



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
                          // or keysplit instrument names

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
    uint8_t midi_key = 0;
    uint8_t velocity = 0;

    double phase = 0.0;
    double phase_increment = 0.0;

    float amplitude = 0.0f;
    bool releasing = false;

    float getNextSample();
    void noteOn(uint8_t key, uint8_t vel);
    void noteOff();
};

#endif // SOUNDTYPES_H
