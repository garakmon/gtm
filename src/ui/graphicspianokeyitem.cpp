#include "graphicspianokeyitem.h"

#include <QPainter>
#include <QWidget>
#include <QGraphicsSceneMouseEvent>



GraphicsPianoKeyItem::GraphicsPianoKeyItem(int note) : QGraphicsItem() {
    this->m_note = note;
}

QRectF GraphicsPianoKeyItem::boundingRect() const {
    return QRectF(this->x(), this->y(), this->dimensions().width(), this->dimensions().height());
}

//! TODO: move to util/
bool is_note_c(int note) {
    return !(note % 12);
}

void GraphicsPianoKeyItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    //
    // int width = 20;
    // int height = 5;
    // int x = 0;
    // int y = this->m_note * 10;

    QSize key_dimensions = this->dimensions();

    painter->setPen(QColor(24, 25, 22));

    //QColor color(0, 201, 114);

    //painter->setPen(Qt::black);
    QColor key_color = this->m_color;
    painter->setBrush(key_color);

    painter->drawRoundedRect(this->x(), this->y(), key_dimensions.width(), key_dimensions.height(), 1, 2);

    if (is_note_c(this->m_note)) {
        //
        QString c_num = QString("C") + QString::number(this->m_note / 12 - 1);
        painter->setPen(Qt::gray);
        int string_size = QFontMetrics(painter->font()).boundingRect(c_num).width();
        painter->drawText(this->x() + key_dimensions.width() - string_size - 2, this->y() + key_dimensions.height() - 1, c_num);
    }
}

void GraphicsPianoKeyItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    // try to play this note
    qDebug() << "note:" << this->m_note;
    event->accept();
}
