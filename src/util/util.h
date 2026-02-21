#pragma once
#ifndef UTIL_H
#define UTIL_H

#include <QMap>
#include <QString>

#include <cstdint>



struct NotePos {
    int y;
    int height;
};

enum class InstrumentTypeAbbrevMode {
    Display,
    Playback,
};

struct Instrument;
struct KeysplitTable;
struct VoiceGroup;

bool isNoteWhite(int note);
bool isNoteC(int note);
QString instrumentTypeAbbrev(
    const Instrument *inst,
    InstrumentTypeAbbrevMode mode = InstrumentTypeAbbrevMode::Display
);
const Instrument *resolveInstrumentForKey(
    const Instrument *inst,
    uint8_t key,
    const QMap<QString, VoiceGroup> *all_voicegroups,
    const QMap<QString, KeysplitTable> *keysplit_tables = nullptr,
    const VoiceGroup **out_vg = nullptr
);
QString noteValueToString(int note);
QString noteValueToString(int note, int sharps_flats, bool is_minor);
int countSetBits(int num);
int whiteNoteToY(int note);
int blackNoteToY(int note);
NotePos scoreNotePosition(int note); // <y value, bar height> pairs from note

#endif // UTIL_H
