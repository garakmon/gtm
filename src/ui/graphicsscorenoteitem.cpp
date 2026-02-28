#include "ui/graphicsscorenoteitem.h"

#include "deps/midifile/MidiEvent.h"
#include "ui/colors.h"
#include "ui/pianoroll.h"
#include "util/constants.h"
#include "util/util.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>



GraphicsScoreNoteItem::GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, int row, smf::MidiEvent *on, smf::MidiEvent *off)
 : GraphicsMidiEventItem(track, on), m_row(row) {
    this->m_piano_roll = piano_roll;
    this->m_note_off = off;
    this->updatePosition();

    this->setFlags(QGraphicsItem::ItemIsSelectable);
}

QRectF GraphicsScoreNoteItem::boundingRect() const {
    return QRectF(0, 0, this->dimensions().width(), this->dimensions().height());
}

void GraphicsScoreNoteItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if (option->state & QStyle::State_Selected) {
        static QPen highlighter(Qt::white, 3, Qt::SolidLine);
        painter->setPen(highlighter);
    }
    painter->setBrush(this->color());
    painter->drawRoundedRect(this->boundingRect(), 3, 2);
}

QColor GraphicsScoreNoteItem::color() {
    int index = m_row < g_max_num_tracks ? m_row : g_max_num_tracks - 1;
    return ui_track_color_array[index];
}

QSize GraphicsScoreNoteItem::dimensions() const {
    return QSize(this->m_event->getTickDuration() * ui_tick_x_scale, ui_score_line_height);
};

QPoint GraphicsScoreNoteItem::updatePosition() {
    int note = this->m_event->getKeyNumber();
    int x = this->m_event->tick * ui_tick_x_scale;
    int y = scoreNotePosition(note).y + 2; //! INVESTIGATE: why is +2 necessary?

    QPoint pos(x, y);
    this->setPos(pos);
    return pos;
};

QVariant GraphicsScoreNoteItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case QGraphicsItem::ItemSelectedHasChanged:
        if (value.toBool()) {
            emit this->m_piano_roll->eventItemSelected(this->m_event);
        }
        break;
    case QGraphicsItem::ItemPositionHasChanged:
        break;
    default:
        break;
    }

    return QGraphicsItem::itemChange(change, value);
}
