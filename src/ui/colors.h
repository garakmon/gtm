#pragma once
#ifndef COLORS_H
#define COLORS_H

#include <QColor>

#include "util/constants.h"
#include "util/theme.h"



// Track Colors
extern QColor ui_track_color_array[g_max_num_tracks];
// TODO: selected color array, deselected / inactive color array


// Piano Keys


// Piano Roll
extern QColor ui_color_piano_roll_bg;
extern QColor ui_color_score_line_dark;
extern QColor ui_color_score_line_light;
extern QColor ui_color_piano_roll_tick_mark;

// Measure Roll
extern QColor ui_color_measure_timestamps;
extern QColor ui_color_measure_guide;

extern QColor ui_meta_event_timesignature;
extern QColor ui_meta_event_tempo;
extern QColor ui_meta_event_keysignature;
extern QColor ui_meta_event_marker;
extern QColor ui_event_program;
extern QColor ui_event_volume;
extern QColor ui_event_pan;
extern QColor ui_event_expression;
extern QColor ui_event_pitch;
extern QColor ui_event_control_other;

void applyThemeColors(const ThemePalette &palette);

#endif // COLORS_H
