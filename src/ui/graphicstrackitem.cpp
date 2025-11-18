#include "graphicstrackitem.h"

#include <QPainter>

#include "graphicsscorenoteitem.h"
#include "constants.h"
#include "colors.h"
#include "MidiEventList.h"


// ui_track_color_array
// const int ui_track_item_height = 30;
// const int ui_track_item_width = 300;

GraphicsTrackItem::GraphicsTrackItem(int track, int row, QGraphicsItem *parent) : QGraphicsItem(parent) {
    this->m_track = track;
    this->m_row = row;
    int color_index = row < g_max_num_tracks ? row : g_max_num_tracks - 1;
    this->m_color = ui_track_color_array[color_index];
    this->m_color_light = ui_track_color_array[color_index].lighter(150);
}

QRectF GraphicsTrackItem::boundingRect() const {
    return QRectF(0, ui_track_item_height * this->m_row, ui_track_item_width, ui_track_item_height);
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



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackMetaEventItem::GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event = event;
}

QRectF GraphicsTrackMetaEventItem::boundingRect() const {
    return QRectF(0, 0, this->m_event->getTickDuration() * ui_tick_x_scale, ui_track_item_height / 2);
}

void GraphicsTrackMetaEventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(this->m_parent_track->color());
    painter->drawRoundedRect(this->boundingRect(), 5, 5);
}



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackRollManager::GraphicsTrackRollManager(smf::MidiEventList *event_list, GraphicsTrackItem *parent_track)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event_list = event_list;
}

QRectF GraphicsTrackRollManager::boundingRect() const {
    return QRectF(0, ui_track_item_height * this->m_parent_track->row(), 10000, ui_track_item_height);
}

void GraphicsTrackRollManager::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(QColor::fromHsl(this->m_parent_track->color().hslHue(), this->m_parent_track->color().hslSaturation(), 220));
    painter->setPen(QPen(this->m_parent_track->color()));
    painter->drawRoundedRect(this->boundingRect(), 5, 5);
}
