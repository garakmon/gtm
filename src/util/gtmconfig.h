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
    QString palette = "default";
    QString theme = "default";
};

#endif // GTMCONFIG_H
