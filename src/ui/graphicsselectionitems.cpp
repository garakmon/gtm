#include "ui/graphicsselectionitems.h"

#include <QPainter>



static void drawSelectionOutline(QPainter *painter, const QPainterPath &path) {
    QPen black_pen(Qt::black, 1.0, Qt::CustomDashLine);
    black_pen.setCosmetic(true);
    black_pen.setDashPattern({3.0, 3.0});
    black_pen.setDashOffset(0.0);

    QPen white_pen(Qt::white, 1.0, Qt::CustomDashLine);
    white_pen.setCosmetic(true);
    white_pen.setDashPattern({3.0, 3.0});
    white_pen.setDashOffset(3.0);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(black_pen);
    painter->drawPath(path);
    painter->setPen(white_pen);
    painter->drawPath(path);
}



GraphicsSelectionRectItem::GraphicsSelectionRectItem() {
    this->setZValue(1000.0);
}

QRectF GraphicsSelectionRectItem::boundingRect() const {
    return m_rect.normalized().adjusted(-1.0, -1.0, 1.0, 1.0);
}

void GraphicsSelectionRectItem::paint(QPainter *painter,
                                      const QStyleOptionGraphicsItem *option,
                                      QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QPainterPath path;
    path.addRect(m_rect.normalized());
    drawSelectionOutline(painter, path);
}

void GraphicsSelectionRectItem::setSelectionRect(const QRectF &rect) {
    if (m_rect == rect) {
        return;
    }

    this->prepareGeometryChange();
    m_rect = rect;
    this->update();
}



GraphicsSelectionPathItem::GraphicsSelectionPathItem() {
    this->setZValue(1000.0);
}

QRectF GraphicsSelectionPathItem::boundingRect() const {
    return m_path.boundingRect().adjusted(-1.0, -1.0, 1.0, 1.0);
}

void GraphicsSelectionPathItem::paint(QPainter *painter,
                                      const QStyleOptionGraphicsItem *option,
                                      QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    drawSelectionOutline(painter, m_path);
}

void GraphicsSelectionPathItem::setSelectionPath(const QPainterPath &path) {
    if (m_path == path) {
        return;
    }

    this->prepareGeometryChange();
    m_path = path;
    this->update();
}
