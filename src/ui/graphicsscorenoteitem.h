#pragma once
#ifndef GRAPHICSSCOREITEMS_H
#define GRAPHICSSCOREITEMS_H

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>



namespace smf { class MidiEvent; }
class PianoRoll;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsMidiEventItem is a base graphics item for ui objects that correspond directly
/// to a MidiEvent. It stores the shared track index and event pointer used by the
/// other event-backed items in this file.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsMidiEventItem : public QGraphicsItem {
public:
    GraphicsMidiEventItem(int track, smf::MidiEvent *event) : QGraphicsItem(nullptr) {
        this->m_track = track;
        this->m_event = event;
    }

    virtual ~GraphicsMidiEventItem() {}

    virtual QColor color() { return QColor(); }
    virtual QSize dimensions() const { return QSize(0, 0); }
    virtual QPoint updatePosition() { return QPoint(0, 0); }
    int track() const { return m_track; }
    smf::MidiEvent *event() const { return m_event; }

protected:
    int m_track = 0;
    smf::MidiEvent *m_event = nullptr;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsNoteVisualItem is a reusable visual note base for drawing piano-roll notes
/// from simple geometry state.
///
/// It exists mainly because when creating notes through the ui, there may not be a note
/// item which exists yet, but we still need a visual preview of the note to be created.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsNoteVisualItem : public QGraphicsItem {

public:
    GraphicsNoteVisualItem(int track, int row, int tick, int key, int duration_ticks);
    ~GraphicsNoteVisualItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QColor color() const;
    QSize dimensions() const;
    QPoint updatePosition();

    int track() const { return m_track; }
    int row() const { return m_row; }
    int tick() const { return m_tick; }
    int key() const { return m_key; }
    int tickDuration() const { return m_duration_ticks; }
    void setTick(int tick) { m_tick = tick; }
    void setKey(int key) { m_key = key; }
    void setTickDuration(int duration_ticks);

protected:
    int m_track = 0;
    int m_row = 0;
    int m_tick = 0;
    int m_key = 0;
    int m_duration_ticks = 0;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsScoreNoteItem is the interactive piano-roll note item backed by a real
/// note-on and note-off MidiEvent pair. It reuses GraphicsNoteVisualItem for drawing.
/// Note selection, drag-to-move, and drag-to-resize behavior are handled here too.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsScoreNoteItem : public GraphicsMidiEventItem {

public:
    GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, int row, smf::MidiEvent *on, smf::MidiEvent *off);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QColor color() override;
    QSize dimensions() const override;
    QPoint updatePosition() override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    smf::MidiEvent *noteOn() const { return m_event; }
    smf::MidiEvent *noteOff() const { return m_note_off; }
    int tickDuration() const;
    bool isStartResizeHandle(const QPointF &pos) const;
    bool isEndResizeHandle(const QPointF &pos) const;
    bool isCloserToStartEdge(const QPointF &pos) const;
    void setPreviewDurationTicks(int duration_ticks);
    void clearPreviewDurationTicks();

private:
    int m_preview_duration_ticks = -1;
    smf::MidiEvent *m_note_off = nullptr;

    PianoRoll *m_piano_roll = nullptr;
    GraphicsNoteVisualItem m_visual_item;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsPreviewNoteItem is a temporary note-shaped graphics item used for edit
/// previews, such as creating a new note before it exists in the song data.
///
/// It is purely visual and does not point at a MidiEvent.
///
/// !TODO
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsPreviewNoteItem : public GraphicsNoteVisualItem {

public:
    GraphicsPreviewNoteItem(int track, int row, int tick, int key, int duration_ticks);
    ~GraphicsPreviewNoteItem() override = default;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsTrackEventItem is a placeholder base for track-level midi event graphics that
/// are not piano-roll notes. It remains event-backed, but is separate from the note item
/// hierarchy because those events are drawn and interacted with differently.
///
/// !TODO
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsTrackEventItem : public GraphicsMidiEventItem {

public:
    GraphicsTrackEventItem();

private:
    int m_event_type;
};

#endif // GRAPHICSSCOREITEMS_H
