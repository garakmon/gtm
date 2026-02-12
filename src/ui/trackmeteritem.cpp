#include "ui/trackmeteritem.h"

#include <QPainter>
#include <QtMath>

GraphicsTrackMeterItem::GraphicsTrackMeterItem(const QSizeF &size, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_size(size) {
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

QRectF GraphicsTrackMeterItem::boundingRect() const {
    return QRectF(0.0, 0.0, m_size.width(), m_size.height());
}

void GraphicsTrackMeterItem::setLevels(float left, float right) {
    float cl = qBound(0.0f, left, 1.0f);
    float cr = qBound(0.0f, right, 1.0f);
    if (qAbs(cl - m_last_left) < 0.001f && qAbs(cr - m_last_right) < 0.001f) return;
    m_last_left = cl;
    m_last_right = cr;
    m_level_left = cl;
    m_level_right = cr;
    update();
}

void GraphicsTrackMeterItem::setSegmentsPerSide(int segments) {
    m_segments_per_side = qMax(1, segments);
    update();
}

void GraphicsTrackMeterItem::setColors(const QColor &base) {
    m_color_on = base;
    m_color_off = base.darker(260);
    update();
}

void GraphicsTrackMeterItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, false);

    int segments = qMax(1, m_segments_per_side);
    float gap = 1.0f;
    float center_gap = 2.0f;
    float center_x = m_size.width() * 0.5f;

    float side_width = (m_size.width() - center_gap * 2.0f - 1.0f) * 0.5f;
    float total_gap = (segments - 1) * gap;
    float seg_w = (side_width - total_gap) / segments;
    if (seg_w < 1.0f) seg_w = 1.0f;

    int lit_left = qBound(0, static_cast<int>(qRound(m_level_left * segments)), segments);
    int lit_right = qBound(0, static_cast<int>(qRound(m_level_right * segments)), segments);

    // Center line
    painter->fillRect(QRectF(center_x, 0.0f, 1.0f, m_size.height()), m_color_off);

    // Left segments (from center outward)
    float x_left = center_x - center_gap - seg_w;
    for (int i = 0; i < segments; ++i) {
        QColor color = (i < lit_left) ? m_color_on : m_color_off;
        painter->fillRect(QRectF(x_left, 0.0f, seg_w, m_size.height()), color);
        x_left -= seg_w + gap;
    }

    // Right segments (from center outward)
    float x_right = center_x + center_gap;
    for (int i = 0; i < segments; ++i) {
        QColor color = (i < lit_right) ? m_color_on : m_color_off;
        painter->fillRect(QRectF(x_right, 0.0f, seg_w, m_size.height()), color);
        x_right += seg_w + gap;
    }
}
