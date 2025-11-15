#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H



// GBA hardware & mixer limits
const int g_max_num_tracks = 16;
const int g_max_voices = 16; // polypjony limit
 const int g_sample_rate = 44100; // Hz



// MIDI piano
const int g_num_notes_piano = 128; // C-1
const int g_num_white_piano = 75;
const int g_num_black_piano = 53;

const int g_num_notes_per_octave = 12; // A -- G
const int g_num_white_per_octave = 7;
const int g_num_black_per_octave = 5;
const int g_num_notes_per_scale = 8; // do-re-mi-fa-so-la-ti-do
const int g_midi_middle_c = 60;

const unsigned g_white_key_mask = 0b10101001010;
const unsigned g_black_key_mask = 0b01010110101;



// UI
const int ui_score_line_height = 10;

const int ui_piano_key_white_height[g_num_white_per_octave] = {17, 16, 17, 17, 18, 18, 17};
const int ui_piano_key_white_height_sums[g_num_white_per_octave] = {0, 17, 33, 50, 67, 85, 103};
const int ui_piano_keys_octave_height = 120;
const int ui_piano_key_black_height = 12;
const int ui_piano_key_white_width = 98;
const int ui_piano_key_black_width = 60;

const int ui_track_item_height = 30;
const int ui_track_item_width = 98;

const int ui_tick_x_scale = 2;

const int ui_measure_roll_height = 30;



#endif // CONSTANTS_H
