#pragma once
#ifndef TRACKMETERITEM_H
#define TRACKMETERITEM_H

#include <QGraphicsItem>
#include <QColor>
#include <QSizeF>

class GraphicsTrackMeterItem : public QGraphicsItem {
public:
    explicit GraphicsTrackMeterItem(const QSizeF &size, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setLevels(float left, float right);
    void setSegmentsPerSide(int segments);
    void setColors(const QColor &base);

private:
    QSizeF m_size;
    int m_segments_per_side = 12;
    float m_level_left = 0.0f;
    float m_level_right = 0.0f;
    float m_last_left = -1.0f;
    float m_last_right = -1.0f;
    QColor m_color_on = QColor(0x2e, 0xcc, 0x71);
    QColor m_color_off = QColor(0x1d, 0x1d, 0x1d);
};

#endif // TRACKMETERITEM_H
