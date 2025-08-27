#include "util.h"
#include "constants.h"



bool is_note_white(int note) {
    return !((1 << (note % 12)) & g_white_key_mask);
}

int count_set_bits(int n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
}
