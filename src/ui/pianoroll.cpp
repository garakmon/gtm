#include "pianoroll.h"

#include <QGraphicsView>
#include <QEvent>
#include <QWheelEvent>

#include "constants.h"
#include "colors.h"
#include "util.h"



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    //
    this->m_scene_piano.setParent(this);
    this->drawPiano();
    this->drawScoreArea();
}

int white_note_to_y_pos(int note) {
    // int max_y = 21 * g_num_white_piano; // TODO: 21 -> white_key_height ; 15 -> black_key_height

    // int octave = note / g_num_notes_per_octave;
    // int index_in_octave = note % g_num_notes_per_octave;

    // int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));

    // int white_note_index = (octave * g_num_white_per_octave) + white_index;

    // int offset = is_note_white(note) ? 0 : 7;
    // return max_y - white_note_index * 21 / 2 + offset;

    int max_y = ui_score_line_height * g_num_notes_piano;

    int octave = note / g_num_notes_per_octave;
    int index_in_octave = note % g_num_notes_per_octave;

    int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));

    int pos = max_y - (octave * ui_piano_keys_octave_height + ui_piano_key_white_height_sums[white_index] + ui_piano_key_white_height[white_index]);

    qDebug() << "note:" << note << "white_index:" << white_index << "pos:" << pos;

    return pos;
}

int black_note_to_y_pos(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano;
    return max_y - ui_score_line_height * (note + 1);
}

void PianoRoll::drawPiano() {
    for (int i = 0; i < g_num_notes_piano; i++) {
        GraphicsPianoKeyItem *item;
        if (is_note_white(i)) {
            item = new GraphicsPianoKeyItemWhite(i);
            item->setZValue(0);
            item->setPos(0, white_note_to_y_pos(i));
        } else {
            item = new GraphicsPianoKeyItemBlack(i);
            item->setZValue(1);
            item->setPos(0, black_note_to_y_pos(i));
        }
        //item->setPos(0, note_to_y_pos(i));
        this->m_piano_keys.append(item);
        this->m_scene_piano.addItem(item);
    }

    QRectF rect = this->m_scene_piano.itemsBoundingRect();
    this->m_scene_piano.setSceneRect(rect);
}

// TODO::: each index gets a y value, the lines and piano keys use the same value
//  then draw keys centered on that value instead of whatever is happening now?
// could this be a static list for lookup purposes and speed purposes or is programmatic fast enough

void PianoRoll::drawScoreArea() {
    int starting_y = ui_score_line_height * g_num_notes_piano - ui_piano_key_black_height;

    for (int i = 0; i < g_num_notes_piano; i++) {
        QGraphicsRectItem *item;
        // TODO: calculate height of band, for each octave reset
        // scroll_pianoArea
        if (is_note_white(i)) {
            item = new QGraphicsRectItem(0, starting_y, 10000, ui_piano_key_black_height);
            item->setBrush(ui_color_score_line_light);
            item->setPen(ui_color_score_line_dark);
            item->setZValue(-1);
        } else {
            item = new QGraphicsRectItem(0, starting_y, 10000, ui_piano_key_black_height);
            item->setBrush(ui_color_score_line_dark);
            item->setPen(ui_color_score_line_light);
            item->setZValue(-1);
        }
        starting_y -= ui_score_line_height;
        //item->setPos(0, note_to_y_pos(i));
        this->m_score_lines.append(item);
        this->m_scene_roll.addItem(item);
    }

    // QRectF rect = this->m_scene_roll.itemsBoundingRect();
    // this->m_scene_roll.setSceneRect(rect);
    //QRectF rect = this->m_scene_piano.itemsBoundingRect();
    //this->m_scene_roll.setSceneRect(QRectF(0, 0, 1000, rect.height()));
}
