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
