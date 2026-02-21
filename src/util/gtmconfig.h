#pragma once
#ifndef GTMCONFIG_H
#define GTMCONFIG_H

#include <QString>



class GtmConfig {
public:
    static QString fileName();
    static QString defaultPath();
    static GtmConfig loadFromFile(const QString &path, bool *ok = nullptr);
    bool saveToFile(const QString &path) const;

public:
    QString most_recent_project;
    QString recent_song;
    int master_volume = 25;
    QString palette = "default";
    QString theme = "default";
    QString window_geometry;
};

#endif // GTMCONFIG_H
