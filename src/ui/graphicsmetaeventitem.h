#pragma once
#ifndef GRAPHICSMETAEVENTITEM_H
#define GRAPHICSMETAEVENTITEM_H

#include <QGraphicsItem>

namespace smf { class MidiEvent; }

class GraphicsMetaEventItem : public QGraphicsItem {
public:
    enum class MetaType { TimeSignature, Tempo, KeySignature, Marker };

    GraphicsMetaEventItem(smf::MidiEvent *event, MetaType type, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    smf::MidiEvent *event() { return m_event; }
    MetaType metaType() { return m_type; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    smf::MidiEvent *m_event;
    MetaType m_type;
    QString m_label;
};

#endif // GRAPHICSMETAEVENTITEM_H
