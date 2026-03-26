#pragma once
#ifndef GRAPHICSSELECTIONITEMS_H
#define GRAPHICSSELECTIONITEMS_H

#include <QGraphicsItem>
#include <QPainterPath>



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsSelectionRectItem draws the temporary rectangle used by the rectangular
/// selection tool. The outline uses alternating black and white dashes so the box stays
/// visible over both light and dark note colors.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsSelectionRectItem : public QGraphicsItem {
public:
    GraphicsSelectionRectItem();
    ~GraphicsSelectionRectItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    void setSelectionRect(const QRectF &rect);

private:
    QRectF m_rect;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsSelectionPathItem draws the temporary freeform path used by the lasso
/// selection tool. It shares the same alternating outline style as the rectangular
/// selection preview.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsSelectionPathItem : public QGraphicsItem {
public:
    GraphicsSelectionPathItem();
    ~GraphicsSelectionPathItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    void setSelectionPath(const QPainterPath &path);

private:
    QPainterPath m_path;
};

#endif // GRAPHICSSELECTIONITEMS_H
