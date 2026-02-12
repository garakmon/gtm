#include "util/theme.h"

#include <QApplication>
#include <QFile>

static ThemePalette defaultPalette() {
    ThemePalette p;
    p.bg = "#272822";
    p.panel = "#2e2f29";
    p.panel_alt = "#3a3b35";
    p.toolbox = "#2f343d";
    p.text_primary = "#f8f8f2";
    p.text_secondary = "#cfcfc2";
    p.accent = "#a6e22e";
    p.accent_hover = "#b6f03e";
    p.accent_active = "#86c20e";
    p.border = "#49483e";
    p.danger = "#f92672";
    p.scroll_thumb = "#5a5a4f";
    p.scroll_track = "#1f201c";
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
    p.event_program = "#66d9ef";
    p.event_volume = "#a6e22e";
    p.event_pan = "#fd971f";
    p.event_expression = "#e6db74";
    p.event_pitch = "#f92672";
    p.event_control_other = "#b6b6ad";
    return p;
}

static ThemePalette retroPalette() {
    ThemePalette p = defaultPalette();
    p.bg = "#1b1f1d";
    p.panel = "#2a2f2c";
    p.panel_alt = "#343a36";
    p.toolbox = "#d7dec8";
    p.text_primary = "#e6e1c5";
    p.text_secondary = "#b7b19a";
    p.accent = "#8fc66a";
    p.accent_hover = "#7fb85c";
    p.accent_active = "#6aa34c";
    p.border = "#4b524c";
    p.danger = "#d96b5f";
    p.scroll_thumb = "#5a615b";
    p.scroll_track = "#242926";
    return p;
}

ThemePalette paletteByName(const QString &name) {
    const QString key = name.trimmed().toLower();
    if (key.isEmpty() || key == "default") {
        return defaultPalette();
    }
    if (key == "retro") {
        return retroPalette();
    }
    return defaultPalette();
}

QString themeQssPath(const QString &theme_name) {
    const QString name = theme_name.trimmed().toLower();
    if (name == "retro") {
        return QString(":/themes/retro.qss");
    }
    return QString(":/themes/default.qss");
}

QString renderStyleSheet(const ThemePalette &p, const QString &theme_name) {
    QFile file(themeQssPath(theme_name));
    QString qss;
    if (file.open(QIODevice::ReadOnly)) {
        qss = QString::fromUtf8(file.readAll());
        file.close();
    }

    qss.replace("{bg}", p.bg);
    qss.replace("{panel}", p.panel);
    qss.replace("{panel_alt}", p.panel_alt);
    qss.replace("{toolbox}", p.toolbox);
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

void applyTheme(const ThemePalette &palette, const QString &theme_name) {
    QString qss = renderStyleSheet(palette, theme_name);
    if (qApp) {
        qApp->setStyleSheet(qss);
    }
}
