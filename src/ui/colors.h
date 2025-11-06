#pragma once
#ifndef COLORS_H
#define COLORS_H

#include <QColor>

#include "constants.h"



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

#endif // COLORS_H
