#pragma once
#ifndef GRAPHICSSCOREITEMS_H
#define GRAPHICSSCOREITEMS_H

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>



namespace smf {
class MidiEvent;
}
class PianoRoll;

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

    //virtual void remove(); // clear object, call removeEmpties()

protected:
    int m_track = 0;
    smf::MidiEvent *m_event = nullptr;
};



class GraphicsScoreNoteItem : public GraphicsMidiEventItem {

public:
    GraphicsScoreNoteItem(PianoRoll *piano_roll, int track, int row, smf::MidiEvent *on, smf::MidiEvent *off);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QColor color() override;
    QSize dimensions() const override;
    QPoint updatePosition() override; // update & get position

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    smf::MidiEvent *noteOn() const { return m_event; }
    smf::MidiEvent *noteOff() const { return m_note_off; }

private:
    int m_row;
    int m_tick_duration;
    smf::MidiEvent *m_note_off = nullptr;

    PianoRoll *m_piano_roll = nullptr;
};



class GraphicsTrackEventItem : public GraphicsMidiEventItem {

public:
    GraphicsTrackEventItem();

private:
    int m_event_type;
};

#endif // GRAPHICSSCOREITEMS_H
