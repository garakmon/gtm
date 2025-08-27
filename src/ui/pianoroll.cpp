#include "pianoroll.h"

#include <QGraphicsView>
#include <QEvent>
#include <QWheelEvent>

#include "constants.h"
#include "colors.h"



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    //
    this->m_scene_piano.setParent(this);
    this->drawPiano();
    this->drawScoreArea();
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

int white_note_to_y_pos(int note) {
    // int max_y = 21 * g_num_white_piano; // TODO: 21 -> white_key_height ; 15 -> black_key_height

    // int octave = note / g_num_notes_per_octave;
    // int index_in_octave = note % g_num_notes_per_octave;

    // int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));

    // int white_note_index = (octave * g_num_white_per_octave) + white_index;

    // int offset = is_note_white(note) ? 0 : 7;
    // return max_y - white_note_index * 21 / 2 + offset;

    int max_y = 10 * g_num_notes_piano;

    const int white_height_sum[g_num_white_per_octave] = {0, 17, 33, 50, 67, 85, 103};
    const int white_height[g_num_white_per_octave] = {17, 16, 17, 17, 18, 18, 17};

    int octave = note / g_num_notes_per_octave;
    int index_in_octave = note % g_num_notes_per_octave;

    int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));

    int pos = max_y - (octave * 120 + white_height_sum[white_index] + white_height[white_index]);

    qDebug() << "note:" << note << "white_index:" << white_index << "pos:" << pos;

    return pos;
}

int black_note_to_y_pos(int note) {
    int max_y = 10 * g_num_notes_piano;
    return max_y - 10 * (note + 1);
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

    //QRectF rect = this->m_scene_piano.itemsBoundingRect();
    //this->m_scene_roll.setSceneRect(QRectF(0, 0, 1000, rect.height()));

    QColor color_dark = QColor(0x6c9193);
    QColor color_light = QColor(0x99b3b4);

    int starting_y = 10 * g_num_notes_piano - 12;

    for (int i = 0; i < g_num_notes_piano; i++) {
        QGraphicsRectItem *item;
        // TODO: calculate height of band, for each octave reset
        // scroll_pianoArea
        if (is_note_white(i)) {
            item = new QGraphicsRectItem(0, starting_y, 1000, 12);
            item->setBrush(color_light);
            item->setPen(color_dark);
            item->setZValue(-1);
        } else {
            item = new QGraphicsRectItem(0, starting_y, 1000, 12);
            item->setBrush(color_dark);
            item->setPen(color_light);
            item->setZValue(-1);
        }
        starting_y -= 10;
        //item->setPos(0, note_to_y_pos(i));
        this->m_score_lines.append(item);
        this->m_scene_roll.addItem(item);
    }

    // QRectF rect = this->m_scene_roll.itemsBoundingRect();
    // this->m_scene_roll.setSceneRect(rect);
    //QRectF rect = this->m_scene_piano.itemsBoundingRect();
    //this->m_scene_roll.setSceneRect(QRectF(0, 0, 1000, rect.height()));
}

bool PianoRoll::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        qDebug() << "Wheel event detected. Delta:" << wheelEvent->angleDelta();

        event->ignore();

        // Intercept the event and prevent the scroll area from handling it
        return true; 

        // Allow the event to continue propagating
        // return false; 
    }

    // Pass the event to the base class
    //return false;
    return QObject::eventFilter(watched, event);
}



// void GraphicsSceneScore::drawBackground(QPainter *painter, const QRectF &rect) {
//     // Define your two colors
//     QColor color_dark = QColor(0x6c9193);
//     QColor color_light = QColor(0x99b3b4);

//     int key_band_height = 12; 

//     // Calculate the start and end of the drawing area
//     int startY = static_cast<int>(rect.top());
//     int endY = static_cast<int>(rect.bottom());

//     painter->setPen(Qt::NoPen);

//     for (int y = startY; y < endY; y += key_band_height) {
//         // Determine the current color based on the band index
//         //bool isOdd = (y / bandHeight) % 2;
//         //QColor bandColor = isOdd ? color_light : color_dark;
//         painter->setBrush(QBrush(bandColor));

//         // Draw the rectangle for the band
//         QRectF bandRect(rect.left(), y, rect.width(), key_band_height);
//         painter->drawRect(bandRect.intersected(rect));
//     }
// }
