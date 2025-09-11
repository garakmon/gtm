#pragma once
#ifndef GRAPHICSSCOREITEMS_H
#define GRAPHICSSCOREITEMS_H

#include <QGraphicsItem>
#include <QColor>



namespace smf {
class MidiEvent;
}

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

    //virtual void remove(); // clear object, call removeEmpties()

protected:
    int m_track = 0;
    smf::MidiEvent *m_event = nullptr;
};



class GraphicsScoreNoteItem : public GraphicsMidiEventItem {

public:
    GraphicsScoreNoteItem(int track, smf::MidiEvent *on, smf::MidiEvent *off);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QColor color() override;
    QSize dimensions() const override;
    QPoint updatePosition() override; // update & get position

private:
    int m_tick_duration;
    smf::MidiEvent *m_note_off = nullptr;
};



class GraphicsTrackEventItem : public GraphicsMidiEventItem {

public:
    GraphicsTrackEventItem();

private:
    int m_event_type;
};

#endif // GRAPHICSSCOREITEMS_H
