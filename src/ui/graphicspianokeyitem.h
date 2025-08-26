#pragma once
#ifndef GRAPHICSPIANOKEYITEM_H
#define GRAPHICSPIANOKEYITEM_H

#include <QGraphicsItem>

class GraphicsPianoKeyItem : public QGraphicsItem {
public:
    GraphicsPianoKeyItem(int note);

    QRectF boundingRect() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual QSize dimensions() const = 0;
    void setColor(QColor color) { this->m_color = color; }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int m_note = 60; // C4
    QColor m_color;
};



class GraphicsPianoKeyItemBlack : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemBlack(int note) : GraphicsPianoKeyItem(note) {
        this->setColor(Qt::black); // TODO: make more advanced looking with paint() reimplimentation
    }

protected:
    QSize dimensions() const override { return QSize(60, 15); }; // TODO: constants
};



class GraphicsPianoKeyItemWhite : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemWhite(int note) : GraphicsPianoKeyItem(note) {
        this->setColor(Qt::white); // TODO: make more advanced looking with paint() reimplimentation
    }

protected:
    QSize dimensions() const override { return QSize(98, 21); }; // TODO: constants
};

#endif // GRAPHICSPIANOKEYITEM_H
