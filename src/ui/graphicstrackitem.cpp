#include "graphicstrackitem.h"

#include <QPainter>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>

#include "graphicsscorenoteitem.h"
#include "constants.h"
#include "colors.h"
#include "MidiEventList.h"
#include "MidiEvent.h"


// ui_track_color_array
// const int ui_track_item_height = 30;
// const int ui_track_item_width = 300;

GraphicsTrackItem::GraphicsTrackItem(int track, int row, QGraphicsItem *parent) : QGraphicsObject(parent) {
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
    QRectF rect = this->boundingRect();

    painter->setBrush(this->m_color);
    painter->drawRoundedRect(rect, 5, 5);

    // Mute button (the small square)
    QRectF muteRect(5, rect.y() + 5, ui_track_item_height - 10, ui_track_item_height - 10);
    if (m_muted) {
        painter->setBrush(QColor(100, 100, 100));  // gray when muted
    } else {
        painter->setBrush(this->m_color_light);
    }
    painter->drawRoundedRect(muteRect, 3, 3);

    // Draw "M" if muted
    if (m_muted) {
        painter->setPen(Qt::white);
        painter->setFont(QFont("sans-serif", 10, QFont::Bold));
        painter->drawText(muteRect, Qt::AlignCenter, "M");
    }

    // Draw current instrument/voice type info if playing
    if (!m_playing_instrument.isEmpty() || !m_playing_voice_type.isEmpty()) {
        painter->setPen(m_color.lightness() > 100 ? Qt::black : Qt::white);
        painter->setFont(QFont("sans-serif", 8));

        QString info = m_playing_voice_type;
        if (!m_playing_instrument.isEmpty()) {
            info += " " + m_playing_instrument;
        }

        QRectF textRect(ui_track_item_height, rect.y() + 2, rect.width() - ui_track_item_height - 5, rect.height() - 4);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, info);
    }
}

void GraphicsTrackItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QRectF rect = boundingRect();
    QRectF muteRect(5, rect.y() + 5, ui_track_item_height - 10, ui_track_item_height - 10);

    if (muteRect.contains(event->pos())) {
        m_muted = !m_muted;
        update();
        emit muteToggled(m_row, m_muted);  // use row as channel
    }

    QGraphicsObject::mousePressEvent(event);
}

void GraphicsTrackItem::addItem(GraphicsScoreItem *item) {
    //
    this->m_score_items.append(item);
    //item->setColor(this->m_color);
}

void GraphicsTrackItem::setPlayingInfo(const QString &instrument, const QString &voiceType) {
    m_playing_instrument = instrument;
    m_playing_voice_type = voiceType;
    update();  // trigger repaint
}

void GraphicsTrackItem::clearPlayingInfo() {
    m_playing_instrument.clear();
    m_playing_voice_type.clear();
    update();
}



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackMetaEventItem::GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event = event;

    // Build label text based on event type
    if (event->isPatchChange()) {
        m_label = QString("P%1").arg(event->getP1());
    } else if (event->isController()) {
        int cc = event->getP1();
        int val = event->getP2();
        switch (cc) {
        case 7:  m_label = QString("Vol:%1").arg(val); break;
        case 10: m_label = QString("Pan:%1").arg(val); break;
        case 11: m_label = QString("Exp:%1").arg(val); break;
        default: m_label = QString("CC%1:%2").arg(cc).arg(val); break;
        }
    } else if (event->isPitchbend()) {
        int bend = (event->getP2() << 7) | event->getP1();
        m_label = QString("PB:%1").arg(bend - 8192);
    }
}

QRectF GraphicsTrackMetaEventItem::boundingRect() const {
    // Size based on label width
    QFontMetrics fm{QFont{}};
    int width = fm.horizontalAdvance(m_label) + 8;
    return QRectF(0, 0, qMax(width, 30), ui_track_item_height - 4);
}

void GraphicsTrackMetaEventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QRectF rect = boundingRect();
    QColor bg = m_parent_track->color().darker(110);

    painter->setBrush(bg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 3, 3);

    // Text color for contrast
    painter->setPen(bg.lightness() > 100 ? Qt::black : Qt::white);
    painter->setFont(QFont("sans-serif", 8));
    painter->drawText(rect, Qt::AlignCenter, m_label);
}



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackRollManager::GraphicsTrackRollManager(smf::MidiEventList *event_list, GraphicsTrackItem *parent_track)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event_list = event_list;

    // Calculate width from last event
    m_width = 0;
    if (event_list->size() > 0) {
        m_width = (*event_list)[event_list->size() - 1].tick * ui_tick_x_scale;
    }

    // Create child items for meta events (patch change, CC, pitch bend)
    int row_y = ui_track_item_height * parent_track->row();
    for (int i = 0; i < event_list->size(); i++) {
        smf::MidiEvent *event = &(*event_list)[i];

        // Skip note events - only show control/meta events
        if (event->isNoteOn() || event->isNoteOff()) {
            continue;
        }

        if (event->isPatchChange() || event->isController() || event->isPitchbend()) {
            GraphicsTrackMetaEventItem *item = new GraphicsTrackMetaEventItem(event, parent_track);
            item->setParentItem(this);
            // Position at the event's tick and track's row
            item->setPos(event->tick * ui_tick_x_scale, row_y);
        }
    }
}

QRectF GraphicsTrackRollManager::boundingRect() const {
    return QRectF(0, ui_track_item_height * this->m_parent_track->row(), m_width, ui_track_item_height);
}

void GraphicsTrackRollManager::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setBrush(QColor::fromHsl(this->m_parent_track->color().hslHue(), this->m_parent_track->color().hslSaturation(), 220));
    painter->setPen(QPen(this->m_parent_track->color()));
    painter->drawRoundedRect(this->boundingRect(), 5, 5);
}
