#include "theme.h"

#include <QApplication>
#include <QFile>

static ThemePalette defaultPalette() {
    ThemePalette p;
    p.bg = "#202124";
    p.panel = "#2a2c30";
    p.panel_alt = "#32353a";
    p.text_primary = "#e6e6e6";
    p.text_secondary = "#b8b8b8";
    p.accent = "#7fd0ff";
    p.accent_hover = "#5fb8ff";
    p.accent_active = "#3ea0ff";
    p.border = "#3b3f46";
    p.danger = "#ff6b6b";
    p.scroll_thumb = "#4a4f58";
    p.scroll_track = "#24272c";
    p.track_colors = QVector<QString>{
        "#d32f2f", // 0 Crimson Red
        "#f57c00", // 1 Rich Orange
        "#fbc02d", // 2 Golden Yellow
        "#388e3c", // 3 Forest Green
        "#1976d2", // 4 Royal Blue
        "#7b1fa2", // 5 Deep Purple
        "#5d0000", // 6 Maroon
        "#873600", // 7 Burnt Umber
        "#7d6608", // 8 Olive Gold
        "#0a2f10", // 9 Obsidian Green
        "#002366", // 10 Midnight Blue
        "#310a31", // 11 Dark Eggplant
        "#a36464", // 12 Dusty Rose
        "#b08d6d", // 13 Sandstone
        "#9e9d75", // 14 Sage
        "#6a8e7f"  // 15 Slate Green
    };
    p.piano_roll_bg = "#6c9193";
    p.score_line_dark = "#bbcccd";
    p.score_line_light = "#d2ddde";
    p.piano_roll_tick_mark = "#828e8f";
    p.measure_timestamps = "#b4ffd2";
    p.measure_guide = "#e63946";
    p.meta_timesignature = "#ff00ff";
    p.meta_tempo = "#00ffff";
    p.meta_keysignature = "#1a1a1a";
    p.meta_marker = "#ffff00";
    return p;
}

ThemePalette paletteByName(const QString &name) {
    const QString key = name.trimmed().toLower();
    if (key.isEmpty() || key == "default") {
        return defaultPalette();
    }
    return defaultPalette();
}

QString renderStyleSheet(const ThemePalette &p) {
    QFile file(":/themes/default.qss");
    QString qss;
    if (file.open(QIODevice::ReadOnly)) {
        qss = QString::fromUtf8(file.readAll());
        file.close();
    }

    qss.replace("{bg}", p.bg);
    qss.replace("{panel}", p.panel);
    qss.replace("{panel_alt}", p.panel_alt);
    qss.replace("{text_primary}", p.text_primary);
    qss.replace("{text_secondary}", p.text_secondary);
    qss.replace("{accent}", p.accent);
    qss.replace("{accent_hover}", p.accent_hover);
    qss.replace("{accent_active}", p.accent_active);
    qss.replace("{border}", p.border);
    qss.replace("{danger}", p.danger);
    qss.replace("{scroll_thumb}", p.scroll_thumb);
    qss.replace("{scroll_track}", p.scroll_track);
    return qss;
}

void applyTheme(const ThemePalette &palette) {
    QString qss = renderStyleSheet(palette);
    if (qApp) {
        qApp->setStyleSheet(qss);
    }
}
