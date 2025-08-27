#include "graphicspianokeyitem.h"

#include <QPainter>
#include <QWidget>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsEffect>



GraphicsPianoKeyItem::GraphicsPianoKeyItem(int note) : QGraphicsItem() {
    this->m_note = note;
}

QRectF GraphicsPianoKeyItem::boundingRect() const {
    return QRectF(0, 0, this->dimensions().width(), this->dimensions().height());
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

    painter->drawRoundedRect(0, 0, key_dimensions.width(), key_dimensions.height(), 1, 2);

    if (is_note_c(this->m_note)) {
        //
        QString c_num = QString("C") + QString::number(this->m_note / 12 - 1);
        painter->setPen(Qt::gray);
        int string_size = QFontMetrics(painter->font()).boundingRect(c_num).width();
        painter->drawText(key_dimensions.width() - string_size - 2, key_dimensions.height() - 1, c_num);
    }
}

void GraphicsPianoKeyItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    // try to play this note
    qDebug() << "note:" << this->m_note << "at" << this->pos() << "vs scene:" << this->scenePos();
    qDebug() << "clicked pos:" << event->pos();
    event->accept();
}



GraphicsPianoKeyItemBlack::GraphicsPianoKeyItemBlack(int note) : GraphicsPianoKeyItem(note) {
    this->setColor(Qt::black); // TODO: make more advanced looking with paint() reimplimentation

    QGraphicsDropShadowEffect *black_key_shadow = new QGraphicsDropShadowEffect();
    black_key_shadow->setBlurRadius(10);
    black_key_shadow->setColor(Qt::gray);
    black_key_shadow->setOffset(3, 3);
    this->setGraphicsEffect(black_key_shadow);
}
