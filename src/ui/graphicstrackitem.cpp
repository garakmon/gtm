#include "graphicstrackitem.h"

#include <QPainter>

#include "graphicsscorenoteitem.h"
#include "constants.h"
#include "colors.h"


// ui_track_color_array
// const int ui_track_item_height = 30;
// const int ui_track_item_width = 300;

GraphicsTrackItem::GraphicsTrackItem(int track, QGraphicsItem *parent) : QGraphicsItem(parent) {
    if (track >= g_max_num_tracks) {
        // TODO: log error
        track = g_max_num_tracks - 1;
    }
    this->m_track = track;
    this->m_color = ui_track_color_array[track];
    this->m_color_light = ui_track_color_array[track].lighter(150);
}

QRectF GraphicsTrackItem::boundingRect() const {
    return QRectF(0, ui_track_item_height * this->m_track, ui_track_item_width, ui_track_item_height);
}

void GraphicsTrackItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(this->m_color);
    painter->drawRoundedRect(this->boundingRect(), 5, 5);

    painter->setBrush(this->m_color_light);
    painter->drawRoundedRect(5, this->boundingRect().y() + 5, ui_track_item_height - 10, ui_track_item_height - 10, 3, 3);
}

// Selecting track item highlights all score items of this track
void GraphicsTrackItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    //
}

void GraphicsTrackItem::addItem(GraphicsScoreItem *item) {
    //
    this->m_score_items.append(item);
    //item->setColor(this->m_color);
}

