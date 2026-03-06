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

#endif // STRUCTS_H
