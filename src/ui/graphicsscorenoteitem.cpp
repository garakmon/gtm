#include "graphicsscorenoteitem.h"

#include <QPainter>

#include "colors.h"
#include "constants.h"
#include "util.h"
#include "MidiEvent.h"



GraphicsScoreNoteItem::GraphicsScoreNoteItem(int track, smf::MidiEvent *on, smf::MidiEvent *off)
 : GraphicsMidiEventItem(track, on) {
    // m_event is a note on
    this->m_note_off = off;
    this->updatePosition();
}

QRectF GraphicsScoreNoteItem::boundingRect() const {
    return QRectF(0, 0, this->dimensions().width(), this->dimensions().height());
}

void GraphicsScoreNoteItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(this->color());
    painter->drawRoundedRect(this->boundingRect(), 3, 2);
}

QColor GraphicsScoreNoteItem::color() {
    return ui_track_color_array[m_track];
}

QSize GraphicsScoreNoteItem::dimensions() const {
    return QSize(this->m_event->getTickDuration() * ui_score_note_tick_x_scale, 12);
};

QPoint GraphicsScoreNoteItem::updatePosition() {
    // int index_in_octave = m_note % g_num_notes_per_octave;
    // int white_index = countSetBits(g_black_key_mask & ((1 << index_in_octave) - 1));
    // return QSize(ui_piano_key_white_width, ui_piano_key_white_height[white_index]);
    int note = this->m_event->getKeyNumber();
    int x = this->m_event->tick * ui_score_note_tick_x_scale;
    int y = isNoteWhite(note) ? whiteNoteToY(note) : blackNoteToY(note);

    QPoint pos(x, y);
    this->setPos(pos);
    return pos;
};
