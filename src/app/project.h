#pragma once
#ifndef PROJECT_H
#define PROJECT_H

#include <QMap>
#include <memory>

#include "song.h"



class Project {
    //
public:
    Project();
    ~Project();

public:
    void load();
    void load(QString path);

    std::shared_ptr<Song> activeSong() { return this->m_active_song; }

private:
    //
    QMap<QString, std::shared_ptr<Song>> m_song_table;

    std::shared_ptr<Song> m_active_song;
};

#endif // PROJECT_H
