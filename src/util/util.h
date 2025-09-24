#pragma once
#ifndef UTIL_H
#define UTIL_H



struct NotePos {
    int y;
    int height;
};

bool isNoteWhite(int note);
int countSetBits(int num);
int whiteNoteToY(int note);
int blackNoteToY(int note);
NotePos scoreNotePosition(int note); // <y value, bar height> pairs from note

#endif // UTIL_H
