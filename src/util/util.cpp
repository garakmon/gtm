#include "util/util.h"
#include "util/constants.h"



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

NotePos scoreNotePosition(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano - ui_piano_key_black_height;
    int y = max_y - (note * ui_score_line_height);
    return { .y = y, .height = ui_piano_key_black_height };
}

bool isNoteC(int note) {
    return !(note % 12);
}

static QString s_note_strings_sharp[g_num_notes_per_octave] = {
    "C", QString::fromUtf8("C\u266F"), "D", QString::fromUtf8("D\u266F"), "E",
    "F", QString::fromUtf8("F\u266F"), "G", QString::fromUtf8("G\u266F"), "A", QString::fromUtf8("A\u266F"), "B"
};

static QString s_note_strings_flat[g_num_notes_per_octave] = {
    "C", QString::fromUtf8("D\u266D"), "D", QString::fromUtf8("E\u266D"), "E",
    "F", QString::fromUtf8("G\u266D"), "G", QString::fromUtf8("A\u266D"), "A", QString::fromUtf8("B\u266D"), "B"
};

static int pitchClassFromValue(int value) {
    int pc = value % g_num_notes_per_octave;
    if (pc < 0) pc += g_num_notes_per_octave;
    return pc;
}

QString noteValueToString(int value, int sharps_flats, bool is_minor) {
    Q_UNUSED(is_minor);
    const int pitch_class = pitchClassFromValue(value);
    if (sharps_flats < 0) {
        return s_note_strings_flat[pitch_class];
    }
    return s_note_strings_sharp[pitch_class];
}

QString noteValueToString(int value) {
    return noteValueToString(value, 0, false);
}
