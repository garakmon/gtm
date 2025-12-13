#pragma once
#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QVector>

#include "constants.h"

struct ThemePalette {
    QString bg;
    QString panel;
    QString panel_alt;
    QString text_primary;
    QString text_secondary;
    QString accent;
    QString accent_hover;
    QString accent_active;
    QString border;
    QString danger;
    QString scroll_thumb;
    QString scroll_track;
    QVector<QString> track_colors;
    QString piano_roll_bg;
    QString score_line_dark;
    QString score_line_light;
    QString piano_roll_tick_mark;
    QString measure_timestamps;
    QString measure_guide;
    QString meta_timesignature;
    QString meta_tempo;
    QString meta_keysignature;
    QString meta_marker;
};

ThemePalette paletteByName(const QString &name);
QString themeQssPath(const QString &theme_name);
QString renderStyleSheet(const ThemePalette &palette, const QString &theme_name);
void applyTheme(const ThemePalette &palette, const QString &theme_name);

#endif // THEME_H
