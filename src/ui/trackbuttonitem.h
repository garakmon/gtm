#pragma once
#ifndef TRACKBUTTONITEM_H
#define TRACKBUTTONITEM_H

#include <QGraphicsItem>

class GraphicsTrackItem;

class GraphicsTrackButtonItem : public QGraphicsItem {
public:
    enum class Type { Mute, Solo };

    GraphicsTrackButtonItem(Type type, GraphicsTrackItem *owner);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Type m_type;
    GraphicsTrackItem *m_owner = nullptr;
    bool m_checked = false;
};

#endif // TRACKBUTTONITEM_H
