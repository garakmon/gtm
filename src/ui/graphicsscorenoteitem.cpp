#include "ui/graphicsscorenoteitem.h"

#include "deps/midifile/MidiEvent.h"
#include "ui/colors.h"
#include "ui/pianoroll.h"
#include "util/constants.h"
#include "util/util.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>



GraphicsNoteVisualItem::GraphicsNoteVisualItem(int track, int row, int tick, int key,
                                               int duration_ticks)
    : m_track(track), m_row(row), m_tick(tick), m_key(key), m_duration_ticks(duration_ticks) {
    this->updatePosition();
}

QRectF GraphicsNoteVisualItem::boundingRect() const {
    return QRectF(0, 0, this->dimensions().width(), this->dimensions().height());
}

void GraphicsNoteVisualItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {
    Q_UNUSED(widget);
    if (option->state & QStyle::State_Selected) {
        static QPen highlighter(Qt::white, 3, Qt::SolidLine);
        painter->setPen(highlighter);
    }
    painter->setBrush(this->color());
    painter->drawRoundedRect(this->boundingRect(), 3, 2);
}

QColor GraphicsNoteVisualItem::color() const {
    int index = m_row < g_max_num_tracks ? m_row : g_max_num_tracks - 1;
    return ui_track_color_array[index];
}

QSize GraphicsNoteVisualItem::dimensions() const {
    return QSize(m_duration_ticks * ui_tick_x_scale, ui_score_line_height);
}

QPoint GraphicsNoteVisualItem::updatePosition() {
    int x = m_tick * ui_tick_x_scale;
    int y = scoreNotePosition(m_key).y + 2; // !TODO INVESTIGATE: why is +2 necessary?

    QPoint pos(x, y);
    this->setPos(pos);
    return pos;
}

void GraphicsNoteVisualItem::setTickDuration(int duration_ticks) {
    if (m_duration_ticks == duration_ticks) {
        return;
    }

    this->prepareGeometryChange();
    m_duration_ticks = duration_ticks;
    this->update();
}

GraphicsScoreNoteItem::GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, int row,
                                             smf::MidiEvent *on, smf::MidiEvent *off)
 : GraphicsMidiEventItem(track, on),
   m_note_off(off),
   m_piano_roll(piano_roll),
   m_visual_item(track, row, on->tick, on->getKeyNumber(), on->getTickDuration()) {
    this->updatePosition();

    this->setFlags(QGraphicsItem::ItemIsSelectable);
    this->setAcceptHoverEvents(true);
}

QRectF GraphicsScoreNoteItem::boundingRect() const {
    return m_visual_item.boundingRect();
}

void GraphicsScoreNoteItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    m_visual_item.paint(painter, option, widget);
}

QColor GraphicsScoreNoteItem::color() {
    return m_visual_item.color();
}

QSize GraphicsScoreNoteItem::dimensions() const {
    const int duration_ticks = m_preview_duration_ticks >= 0
                             ? m_preview_duration_ticks
                             : this->m_event->getTickDuration();
    return QSize(duration_ticks * ui_tick_x_scale, ui_score_line_height);
}

QPoint GraphicsScoreNoteItem::updatePosition() {
    m_visual_item.setTick(this->m_event->tick);
    m_visual_item.setKey(this->m_event->getKeyNumber());
    m_visual_item.setTickDuration(this->m_event->getTickDuration());
    const QPoint pos = m_visual_item.updatePosition();
    this->setPos(pos);
    return pos;
}

QVariant GraphicsScoreNoteItem::itemChange(GraphicsItemChange change,
                                           const QVariant &value) {
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
    if (m_piano_roll->isRectSelectEnabled() || m_piano_roll->isLassoSelectEnabled()) {
        this->unsetCursor();
        QGraphicsItem::hoverMoveEvent(event);
        return;
    }

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
    if (m_piano_roll->isRectSelectEnabled()) {
        m_piano_roll->handleRectSelectMousePress(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll->isLassoSelectEnabled()) {
        m_piano_roll->handleLassoSelectMousePress(event->scenePos());
        event->accept();
        return;
    }

    // mac COMMAND key is MetaModifier
    const bool modify_selection = event->modifiers().testFlag(Qt::ControlModifier)
                                  || event->modifiers().testFlag(Qt::MetaModifier);

    if (modify_selection) {
        this->setSelected(!this->isSelected());
        if (!this->isSelected()) {
            event->accept();
            return;
        }
    }
    else if (!this->isSelected()) {
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
    if (m_piano_roll->isRectSelectEnabled()) {
        m_piano_roll->handleRectSelectMouseMove(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll->isLassoSelectEnabled()) {
        m_piano_roll->handleLassoSelectMouseMove(event->scenePos());
        event->accept();
        return;
    }

    m_piano_roll->handleNoteMouseMove(this, event->scenePos());
    event->accept();
}

/**
 * Commit or cancel the current note drag interaction.
 */
void GraphicsScoreNoteItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_piano_roll->isRectSelectEnabled()) {
        m_piano_roll->handleRectSelectMouseRelease(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll->isLassoSelectEnabled()) {
        m_piano_roll->handleLassoSelectMouseRelease(event->scenePos());
        event->accept();
        return;
    }

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
    m_visual_item.setTickDuration(duration_ticks >= 0 ?
                                  duration_ticks : this->m_event->getTickDuration());
    this->update();
}

void GraphicsScoreNoteItem::clearPreviewDurationTicks() {
    this->setPreviewDurationTicks(-1);
}

GraphicsPreviewNoteItem::GraphicsPreviewNoteItem(int track, int row, int tick, int key,
                                                 int duration_ticks)
    : GraphicsNoteVisualItem(track, row, tick, key, duration_ticks) { }
