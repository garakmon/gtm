#pragma once
#ifndef GRAPHICSSCOREITEMS_H
#define GRAPHICSSCOREITEMS_H

#include <QGraphicsItem>
#include <QColor>



namespace smf {
class MidiEvent;
}

class GraphicsScoreItem : public QGraphicsItem {

public:
    GraphicsScoreItem(smf::MidiEvent *event) : QGraphicsItem(nullptr) {
        this->m_event = event;
    }

    void remove(); // clear object, call removeEmpties()
    void setColor(QColor color) { this->m_color = color; }

protected:
    smf::MidiEvent *m_event;
    QColor m_color;
};



class GraphicsScoreNoteItem : public GraphicsScoreItem {

public:
    GraphicsScoreNoteItem();

private:
    int m_tick_duration;
    smf::MidiEvent *m_connected_note;
};



class GraphicsMidiEventItem : public GraphicsScoreItem {

public:
    GraphicsMidiEventItem();

private:
    int m_event_type;
};

#endif // GRAPHICSSCOREITEMS_H
