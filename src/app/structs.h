#pragma once
#ifndef STRUCTS_H
#define STRUCTS_H

#include <QString>



struct NewSongSettings {
    QString title;
    QString constant;
    QString midi_path;
    QString voicegroup;
    QString player;
    int priority = 0;
    int reverb = 0;
    int volume = 0;
};

struct NoteMoveSettings {
    int delta_tick = 0;
    int delta_key = 0;
};

struct NoteResizeSettings {
    int delta_tick = 0;
    bool resize_end = true;
};

struct TimeEditSettings {
    int tick = 0;
    int delta_ticks = 0;
};

struct ControllerEventSettings {
    int track = 0;
    int tick = 0;
    int channel = 0;
    int cc = 0;
    int value = 0;
};

struct ProgramChangeSettings {
    int track = 0;
    int tick = 0;
    int channel = 0;
    int program = 0;
};

struct TempoEventSettings {
    int tick = 0;
    double bpm = 120.0;
};

struct TimeSignatureSettings {
    int tick = 0;
    int num = 4;
    int den = 4;
};

struct KeySignatureSettings {
    int tick = 0;
    int fifths = 0;
    bool minor = false;
};

#endif // STRUCTS_H
