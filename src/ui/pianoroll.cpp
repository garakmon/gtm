#include "pianoroll.h"

#include <QGraphicsView>

#include "constants.h"
#include "colors.h"



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    //
    this->m_scene_piano.setParent(this);
    this->drawPiano();
}

//! TODO: move to util/
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

int note_to_y_pos(int note) {
    int max_y = 21 * g_num_white_piano; // TODO: 21 -> white_key_height ; 15 -> black_key_height

    int octave = note / g_num_notes_per_octave;
    int index_in_octave = note % g_num_notes_per_octave;

    int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));

    int white_note_index = (octave * g_num_white_per_octave) + white_index;

    int offset = is_note_white(note) ? 0 : 7;
    return max_y - white_note_index * 21 / 2 + offset;
}

void PianoRoll::drawPiano() {
    for (int i = 0; i < g_num_notes_piano; i++) {
        GraphicsPianoKeyItem *item;
        if (is_note_white(i)) {
            item = new GraphicsPianoKeyItemWhite(i);
            item->setZValue(0);
        } else {
            item = new GraphicsPianoKeyItemBlack(i);
            item->setZValue(1);
        }
        item->setPos(0, note_to_y_pos(i));
        this->m_piano_keys.append(item);
        this->m_scene_piano.addItem(item);
    }

    QRectF rect = this->m_scene_piano.itemsBoundingRect();
    this->m_scene_piano.setSceneRect(rect);
}
