#pragma once
#ifndef UTIL_H
#define UTIL_H

#include <QString>



struct NotePos {
    int y;
    int height;
};

bool isNoteWhite(int note);
bool isNoteC(int note);
QString noteValueToString(int note);
QString noteValueToString(int note, int sharps_flats, bool is_minor);
int countSetBits(int num);
int whiteNoteToY(int note);
int blackNoteToY(int note);
NotePos scoreNotePosition(int note); // <y value, bar height> pairs from note

#endif // UTIL_H
