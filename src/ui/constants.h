//! TODO: move to util/
#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H



// MIDI piano
// const int g_num_notes_piano = 88; // A0 -- C8
// const int g_num_white_piano = 52;
// const int g_num_black_piano = 36;
const int g_num_notes_piano = 128; // C-1 -- 
const int g_num_white_piano = 75;
const int g_num_black_piano = 53;

const int g_num_notes_per_octave = 12; // A -- G
const int g_num_white_per_octave = 7;
const int g_num_black_per_octave = 5;
const int g_num_notes_per_scale = 8; // do-re-mi-fa-so-la-ti-do
const int g_midi_middle_c = 60;

const unsigned g_white_key_mask = 0b10101001010;
const unsigned g_black_key_mask = 0b01010110101;

#endif // CONSTANTS_H
