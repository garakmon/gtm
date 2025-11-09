#pragma once
#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

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



// Limited version of the Song class, used to get basic info on songs
// without needing to fully load the midi file into memory
struct SongEntry {
    QString title;
    QString voicegroup;
    QString player;
    int type = 0x00;

    int volume = 0x7F;
    int priority = 0x00;
    int reverb = 0x00;

    QString midifile; // idk if necessary, should always be <title>.mid
};

#endif // SOUNDTYPES_H
