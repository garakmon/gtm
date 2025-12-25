#include "trackbuttonitem.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QIcon>

#include "graphicstrackitem.h"

GraphicsTrackButtonItem::GraphicsTrackButtonItem(Type type, GraphicsTrackItem *owner)
    : QGraphicsItem(owner), m_type(type), m_owner(owner) {
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

QRectF GraphicsTrackButtonItem::boundingRect() const {
    return QRectF(0, 0, 16, 12);
}

void GraphicsTrackButtonItem::setChecked(bool checked) {
    if (m_checked == checked) return;
    m_checked = checked;
    update();
}

void GraphicsTrackButtonItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    QColor bg;
    if (m_type == Type::Mute) {
        bg = m_checked ? QColor(0x64, 0x64, 0x64) : m_owner->colorLight();
    } else {
        bg = m_checked ? QColor(0xf2, 0xc9, 0x4c) : m_owner->colorLight();
    }

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(boundingRect(), 3, 3);

    if (m_type == Type::Mute) {
        const char *icon_path = m_checked ? ":/icons/sound-off.svg" : ":/icons/sound-high.svg";
        QIcon icon(icon_path);
        QPixmap pix = icon.pixmap(10, 10);
        painter->drawPixmap(3, 1, pix);
    } else {
        painter->setPen(Qt::black);
        painter->setFont(QFont("sans-serif", 7, QFont::Bold));
        painter->drawText(boundingRect(), Qt::AlignCenter, "S");
    }
}

void GraphicsTrackButtonItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    m_checked = !m_checked;
    update();
    if (m_owner) {
        m_owner->buttonToggled(m_type == Type::Mute, m_checked);
    }
    event->accept();
}
