#include "graphicsscorenoteitem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "colors.h"
#include "constants.h"
#include "util.h"
#include "MidiEvent.h"
#include "pianoroll.h"



GraphicsScoreNoteItem::GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, smf::MidiEvent *on, smf::MidiEvent *off)
 : GraphicsMidiEventItem(track, on) {
    this->m_piano_roll = piano_roll;
    // m_event is a note on
    this->m_note_off = off;
    this->updatePosition();

    this->setFlags(QGraphicsItem::ItemIsSelectable /*| QGraphicsItem::ItemIsMovable */);
}

QRectF GraphicsScoreNoteItem::boundingRect() const {
    return QRectF(0, 0, this->dimensions().width(), this->dimensions().height());
}

void GraphicsScoreNoteItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if (option->state & QStyle::State_Selected) {
        static QPen highlighter(Qt::yellow, 3, Qt::SolidLine);
        highlighter.setColor(this->color().lighter(200));
        painter->setPen(highlighter);
    }
    painter->setBrush(this->color());
    painter->drawRoundedRect(this->boundingRect(), 3, 2);
}

QColor GraphicsScoreNoteItem::color() {
    return ui_track_color_array[m_track];
}

QSize GraphicsScoreNoteItem::dimensions() const {
    return QSize(this->m_event->getTickDuration() * ui_tick_x_scale, ui_score_line_height);
};

QPoint GraphicsScoreNoteItem::updatePosition() {
    int note = this->m_event->getKeyNumber();
    int x = this->m_event->tick * ui_tick_x_scale;
    int y = scoreNotePosition(note).y + 2; // TODO: why +2 necessary?

    QPoint pos(x, y);
    this->setPos(pos);
    return pos;
};

QVariant GraphicsScoreNoteItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case QGraphicsItem::ItemSelectedHasChanged:
        if (value.toBool()) {
            // item has changed to selected
            qDebug() << "item selected, note:" << this->m_event->getKeyNumber();
            emit this->m_piano_roll->eventItemSelected(this->m_event);
        }
        else {
            // cleared selection
        }
        break;
    case QGraphicsItem::ItemPositionHasChanged:
        break;
    default:
        break;
    }

    return QGraphicsItem::itemChange(change, value);
}
