#include "colors.h"



// Track colors
// TODO: QPalette for different shades?
QColor ui_track_color_array[g_max_num_tracks] = {
    QColor(0xd32f2f), // Crimson Red
    QColor(0xf57c00), // Rich Orange
    QColor(0xfbc02d), // Golden Yellow
    QColor(0x388e3c), // Forest Green
    QColor(0x1976d2), // Royal Blue
    QColor(0x7b1fa2), // Deep Purple
    QColor(0x5d0000), // Maroon
    QColor(0x873600), // Burnt Umber
    QColor(0x7d6608), // Olive Gold
    QColor(0x0a2f10), // Obsidian Green
    QColor(0x002366), // Midnight Blue
    QColor(0x310a31), // Dark Eggplant
    QColor(0xa36464), // Dusty Rose
    QColor(0xb08d6d), // Sandstone
    QColor(0x9e9d75), // Sage
    QColor(0x6a8e7f)  // Slate Green
};


// Piano Keys


// Piano Roll
QColor ui_color_piano_roll_bg = QColor(0x6c9193);
QColor ui_color_score_line_dark = QColor(0xbbcccd);
QColor ui_color_score_line_light = QColor(0xd2ddde);
QColor ui_color_piano_roll_tick_mark = QColor(0x828e8f);

// Measure Roll
QColor ui_color_measure_timestamps = QColor(0xb4ffd2);
QColor ui_color_measure_guide = QColor(0xe63946);

QColor ui_meta_event_timesignature = QColor(0xff00ff);
QColor ui_meta_event_tempo = QColor(0x00ffff);
QColor ui_meta_event_keysignature = QColor(0x1a1a1a);
QColor ui_meta_event_marker = QColor(0xffff00);
