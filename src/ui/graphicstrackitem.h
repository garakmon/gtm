#pragma once

#ifndef GRAPHICSTRACKITEM_H
#define GRAPHICSTRACKITEM_H

// Track item:
//   track color
//   track number (0 indexed)
//   number of events
//   mute / unmute track buttons
//   instrument
//   volume
//   pan
//   

#include <QGraphicsItem>



class GraphicsScoreItem;
namespace smf { class MidiEventList; }
namespace smf { class MidiEvent; }

class GraphicsTrackItem : public QGraphicsItem {
    //
public:
    //
    GraphicsTrackItem(int track, QGraphicsItem *parent = nullptr);


    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Selecting track item highlights all score items of this track
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    QColor color() { return this->m_color; }
    int track() { return this->m_track; }

    void addItem(GraphicsScoreItem *item);

    // int numEvents() { return this->m_note_items.size() + this->m_event_items.size(); }

private:
    QColor m_color;
    QColor m_color_light;
    int m_track;

    QList<GraphicsScoreItem *> m_score_items;

    // QVector<GraphicsScoreNoteItem *> m_note_items;
    // QVector<other event items>
};



///
/// Meta Items
///
class GraphicsTrackMetaEventItem : public QGraphicsItem {
    //
public:
    GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    //
    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEvent *m_event = nullptr;
};



///
/// GraphicsTrackRollManager manages all meta events in a track
///
class GraphicsTrackRollManager : public QGraphicsItem {
    //
public:
    GraphicsTrackRollManager(smf::MidiEventList *event_list, GraphicsTrackItem *parent_track);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    //
    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEventList *m_event_list = nullptr;
};

#endif // GRAPHICSTRACKITEM_H
