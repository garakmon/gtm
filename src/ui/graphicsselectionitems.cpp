#include "ui/graphicsselectionitems.h"

#include <QPainter>

#include <algorithm>



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
                                      const QStyleOptionGraphicsItem *, QWidget *) {
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
                                      const QStyleOptionGraphicsItem *, QWidget *) {
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



GraphicsTimeRangeSelectItem::GraphicsTimeRangeSelectItem() {
    this->setZValue(1000.0);
}

QRectF GraphicsTimeRangeSelectItem::boundingRect() const {
    const qreal left = std::min(m_start_x, m_end_x);
    const qreal right = std::max(m_start_x, m_end_x);
    return QRectF(left, 0.0, right - left, m_height).adjusted(-1.0, -1.0, 1.0, 1.0);
}

void GraphicsTimeRangeSelectItem::paint(QPainter *painter,
                                        const QStyleOptionGraphicsItem *, QWidget *) {
    const qreal left = std::min(m_start_x, m_end_x);
    const qreal right = std::max(m_start_x, m_end_x);
    const QRectF fill_rect(left, 0.0, right - left, m_height);

    painter->setBrush(QColor(0, 0, 0, 64));
    painter->setPen(Qt::NoPen);
    painter->drawRect(fill_rect);

    QPainterPath left_path;
    left_path.moveTo(left, 0.0);
    left_path.lineTo(left, m_height);
    drawSelectionOutline(painter, left_path);

    QPainterPath right_path;
    right_path.moveTo(right, 0.0);
    right_path.lineTo(right, m_height);
    drawSelectionOutline(painter, right_path);
}

void GraphicsTimeRangeSelectItem::setRange(qreal start_x, qreal end_x, qreal height) {
    if (m_start_x == start_x && m_end_x == end_x && m_height == height) {
        return;
    }

    this->prepareGeometryChange();
    m_start_x = start_x;
    m_end_x = end_x;
    m_height = height;
    this->update();
}
