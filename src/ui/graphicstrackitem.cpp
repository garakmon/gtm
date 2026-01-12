#include "graphicstrackitem.h"

#include <QPainter>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include "graphicsscorenoteitem.h"
#include "constants.h"
#include "colors.h"
#include "trackmeteritem.h"
#include "trackbuttonitem.h"
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

    const int row_y = ui_track_item_height * this->m_row;
    const int num_block_w = 20;
    const int num_block_x = 5;
    const int buttons_x = num_block_x + num_block_w + 3;

    const int button_h = 12;
    const int button_gap = 2;
    const int button_stack_h = button_h + button_gap + button_h;
    const int buttons_y = row_y + (ui_track_item_height - button_stack_h) / 2;

    // Mute button (graphics item)
    m_mute_button = new GraphicsTrackButtonItem(GraphicsTrackButtonItem::Type::Mute, this);
    m_mute_button->setPos(buttons_x, buttons_y);
    m_mute_button->setZValue(2);

    // Solo button (graphics item)
    m_solo_button = new GraphicsTrackButtonItem(GraphicsTrackButtonItem::Type::Solo, this);
    m_solo_button->setPos(buttons_x, buttons_y + button_h + button_gap);
    m_solo_button->setZValue(2);

    // Centered stereo meter (agbplay-like), lightweight graphics item
    constexpr bool k_enable_track_meters = true;
    if (k_enable_track_meters) {
        int meter_x = buttons_x + 16 + 6;
        int meter_y = row_y + (ui_track_item_height / 2) + 4;
        m_meter_item = new GraphicsTrackMeterItem(QSizeF(44, 10), this);
        m_meter_item->setColors(m_color_light);
        m_meter_item->setPos(meter_x, meter_y);
        m_meter_item->setZValue(2);
    }

    updateMuteButton();
    updateSoloButton();

    updateMuteButton();
    updateSoloButton();
}

QRectF GraphicsTrackItem::boundingRect() const {
    return QRectF(0, ui_track_item_height * this->m_row, ui_track_item_width, ui_track_item_height);
}

void GraphicsTrackItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QRectF rect = this->boundingRect();

    painter->setBrush(this->m_color);
    painter->drawRoundedRect(rect, 5, 5);

    // Track number block (left)
    QRectF numRect(5, rect.y() + 4, 20, ui_track_item_height - 8);
    painter->setBrush(this->m_color_light);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(numRect, 3, 3);
    painter->setPen(Qt::black);
    painter->setFont(QFont("sans-serif", 9, QFont::Bold));
    painter->drawText(numRect, Qt::AlignCenter, QString::number(m_row));

    // Draw current voice type info if playing (top half, above meter)
    if (!m_playing_voice_type.isEmpty()) {
        painter->setPen(m_color.lightness() > 100 ? Qt::black : Qt::white);
        painter->setFont(QFont("sans-serif", 7));

        const QString info = m_playing_voice_type;
        const int num_block_w = 20;
        const int num_block_x = 5;
        const int buttons_x = num_block_x + num_block_w + 3;
        const int meter_x = buttons_x + 16 + 6;
        const int meter_w = 44;

        QRectF textRect(meter_x, rect.y() + 2, meter_w, (rect.height() / 2) - 2);
        painter->drawText(textRect, Qt::AlignCenter, info);
    }
}

void GraphicsTrackItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsObject::mousePressEvent(event);
}

void GraphicsTrackItem::addItem(GraphicsScoreItem *item) {
    //
    this->m_score_items.append(item);
    //item->setColor(this->m_color);
}

void GraphicsTrackItem::setPlayingInfo(const QString &voiceType) {
    if (voiceType == m_last_voice_type) return;
    m_last_voice_type = voiceType;
    m_playing_voice_type = voiceType;
    update();  // trigger repaint
}

void GraphicsTrackItem::clearPlayingInfo() {
    if (m_last_voice_type.isEmpty()) return;
    m_last_voice_type.clear();
    m_playing_voice_type.clear();
    update();
}

bool GraphicsTrackItem::isSoloed() const {
    return m_solo_button && m_solo_button->isChecked();
}

void GraphicsTrackItem::setMuted(bool muted) {
    m_muted = muted;
    if (m_mute_button) m_mute_button->setChecked(muted);
    updateMuteButton();
    update();
}

void GraphicsTrackItem::setSoloed(bool soloed) {
    if (m_solo_button) m_solo_button->setChecked(soloed);
    updateSoloButton();
    update();
}

void GraphicsTrackItem::setMeterLevels(float left, float right) {
    if (m_meter_item) {
        m_meter_item->setLevels(left, right);
    }
}

void GraphicsTrackItem::updateMuteButton() {
    if (m_mute_button) m_mute_button->update();
}

void GraphicsTrackItem::updateSoloButton() {
    if (m_solo_button) m_solo_button->update();
}

void GraphicsTrackItem::buttonToggled(bool isMuteButton, bool checked) {
    if (isMuteButton) {
        m_muted = checked;
        updateMuteButton();
        update();
        emit muteToggled(m_row, m_muted);
    } else {
        updateSoloButton();
        update();
        emit soloToggled(m_row, checked);
    }
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
