#pragma once
#ifndef PROJECT_H
#define PROJECT_H

#include <QMap>
#include <memory>

#include "song.h"



class Project {
public:
    Project();
    ~Project();

public:
    void load();
    void load(QString path);

    std::shared_ptr<Song> activeSong() { return this->m_active_song; }

    // direct_sound_data.inc mappings
    void addSampleMapping(const QString &label, const QString &path);
    QString getSamplePath(const QString &label) const;

private:
    std::shared_ptr<Song> m_active_song;

    // containers populated during load
    QMap<QString, std::shared_ptr<Song>> m_song_table;
    QMap<QString, QString> m_sample_map;

    friend class ProjectInterface;
};

#endif // PROJECT_H
