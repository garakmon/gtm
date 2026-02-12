#pragma once
#ifndef GRAPHICSPIANOKEYITEM_H
#define GRAPHICSPIANOKEYITEM_H

#include <QGraphicsItem>

#include "util/constants.h"



class GraphicsPianoKeyItem : public QGraphicsItem {
public:
    GraphicsPianoKeyItem(int note);

    QRectF boundingRect() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual QSize dimensions() const = 0;
    void setColor(QColor color) { this->m_color = color; }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

protected:
    int m_note = g_midi_middle_c;
    QColor m_color;
};



class GraphicsPianoKeyItemBlack : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemBlack(int note);
protected:
    QSize dimensions() const override;
};



class GraphicsPianoKeyItemWhite : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemWhite(int note) : GraphicsPianoKeyItem(note) {
        this->setColor(Qt::white); // TODO: make more advanced looking with paint() reimplimentation
    }

protected:
    QSize dimensions() const override;
};

#endif // GRAPHICSPIANOKEYITEM_H
