#include "ui/graphicsscorenoteitem.h"

#include "deps/midifile/MidiEvent.h"
#include "ui/colors.h"
#include "ui/pianoroll.h"
#include "util/constants.h"
#include "util/util.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>



GraphicsScoreNoteItem::GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, int row, smf::MidiEvent *on, smf::MidiEvent *off)
 : GraphicsMidiEventItem(track, on), m_row(row) {
    this->m_piano_roll = piano_roll;
    this->m_note_off = off;
    this->updatePosition();

    this->setFlags(QGraphicsItem::ItemIsSelectable);
    this->setAcceptHoverEvents(true);
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
    const int duration_ticks = m_preview_duration_ticks >= 0
                             ? m_preview_duration_ticks
                             : this->m_event->getTickDuration();
    return QSize(duration_ticks * ui_tick_x_scale, ui_score_line_height);
};

QPoint GraphicsScoreNoteItem::updatePosition() {
    int note = this->m_event->getKeyNumber();
    int x = this->m_event->tick * ui_tick_x_scale;
    int y = scoreNotePosition(note).y + 2; // !TODO INVESTIGATE: why is +2 necessary?

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

/**
 * Show a resize cursor only when hovering a note edge handle.
 */
void GraphicsScoreNoteItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (this->isStartResizeHandle(event->pos()) || this->isEndResizeHandle(event->pos())) {
        this->setCursor(Qt::SizeHorCursor);
    } else {
        this->unsetCursor();
    }

    QGraphicsItem::hoverMoveEvent(event);
}

/**
 * Restore the default cursor when leaving the note.
 */
void GraphicsScoreNoteItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    this->unsetCursor();
    QGraphicsItem::hoverLeaveEvent(event);
}

/**
 * Start either a move drag or a resize drag for this note.
 */
void GraphicsScoreNoteItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (!this->isSelected()) {
        QGraphicsScene *item_scene = this->scene();
        if (item_scene) {
            item_scene->clearSelection();
        }
        this->setSelected(true);
    }

    const bool force_resize = event->modifiers().testFlag(Qt::ShiftModifier);
    bool resize_start = this->isStartResizeHandle(event->pos());
    bool resize_end = !resize_start && this->isEndResizeHandle(event->pos());
    if (force_resize && !resize_start && !resize_end) {
        // holding SHIFT turns the whole note into a resize target
        // this is important because small/short duration notes haven't enough room to
        // detect both drag and resize hovers (drag will always win)
        resize_start = this->isCloserToStartEdge(event->pos());
        resize_end = !resize_start;
    }

    m_piano_roll->handleNoteMousePress(this, event->scenePos(),
                                       resize_start, resize_end);
    event->accept();
}

/**
 * Forward drag updates to the piano roll interaction state.
 */
void GraphicsScoreNoteItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    m_piano_roll->handleNoteMouseMove(this, event->scenePos());
    event->accept();
}

/**
 * Commit or cancel the current note drag interaction.
 */
void GraphicsScoreNoteItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    m_piano_roll->handleNoteMouseRelease(this, event->scenePos());
    event->accept();
}

int GraphicsScoreNoteItem::tickDuration() const {
    return m_event->getTickDuration();
}

bool GraphicsScoreNoteItem::isStartResizeHandle(const QPointF &pos) const {
    return this->dimensions().width() > ui_note_resize_margin * 2
        && pos.x() <= ui_note_resize_margin;
}

bool GraphicsScoreNoteItem::isEndResizeHandle(const QPointF &pos) const {
    return this->dimensions().width() > ui_note_resize_margin * 2
        && pos.x() >= this->dimensions().width() - ui_note_resize_margin;
}

bool GraphicsScoreNoteItem::isCloserToStartEdge(const QPointF &pos) const {
    return pos.x() <= this->dimensions().width() * 0.5;
}

/**
 * Use a temporary duration while previewing note resize.
 */
void GraphicsScoreNoteItem::setPreviewDurationTicks(int duration_ticks) {
    if (m_preview_duration_ticks == duration_ticks) {
        return;
    }

    this->prepareGeometryChange();
    m_preview_duration_ticks = duration_ticks;
    this->update();
}

void GraphicsScoreNoteItem::clearPreviewDurationTicks() {
    this->setPreviewDurationTicks(-1);
}
