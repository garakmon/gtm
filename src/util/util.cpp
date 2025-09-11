#include "util.h"
#include "constants.h"



bool isNoteWhite(int note) {
    return !((1 << (note % 12)) & g_white_key_mask);
}

int countSetBits(int n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
}

int whiteNoteToY(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano;

    int octave = note / g_num_notes_per_octave;
    int index_in_octave = note % g_num_notes_per_octave;

    int white_index = countSetBits(g_black_key_mask & ((1 << index_in_octave) - 1));

    int pos = max_y - (octave * ui_piano_keys_octave_height + ui_piano_key_white_height_sums[white_index] + ui_piano_key_white_height[white_index]);

    return pos;
}

int blackNoteToY(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano;
    return max_y - ui_score_line_height * (note + 1);
}
