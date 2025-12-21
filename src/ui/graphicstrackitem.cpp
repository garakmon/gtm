#include "graphicstrackitem.h"

#include <QPainter>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPushButton>
#include <QIcon>
#include <QSignalBlocker>

#include "graphicsscorenoteitem.h"
#include "constants.h"
#include "colors.h"
#include "meters.h"
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

    // Mute button as a proxy widget (for future expansion with more controls)
    m_mute_button = new QPushButton();
    m_mute_button->setCheckable(true);
    m_mute_button->setFocusPolicy(Qt::NoFocus);
    m_mute_button->setFixedSize(16, 12);
    m_mute_button->setIconSize(QSize(10, 10));

    m_mute_proxy = new QGraphicsProxyWidget(this);
    m_mute_proxy->setWidget(m_mute_button);
    m_mute_proxy->setPos(buttons_x, row_y + 4);
    m_mute_proxy->setZValue(2);

    // Solo button
    m_solo_button = new QPushButton("S");
    m_solo_button->setCheckable(true);
    m_solo_button->setFocusPolicy(Qt::NoFocus);
    m_solo_button->setFixedSize(16, 12);

    m_solo_proxy = new QGraphicsProxyWidget(this);
    m_solo_proxy->setWidget(m_solo_button);
    m_solo_proxy->setPos(buttons_x, row_y + 4 + 12 + 2);
    m_solo_proxy->setZValue(2);

    // Centered stereo meter (agbplay-like)
    m_meter_widget = new CenteredStereoMeter();
    m_meter_widget->setBaseColor(m_color_light);
    m_meter_widget->setSegmentsPerSide(12);
    m_meter_widget->setFixedSize(44, 10);

    m_meter_proxy = new QGraphicsProxyWidget(this);
    m_meter_proxy->setWidget(m_meter_widget);
    int meter_x = buttons_x + 16 + 6;
    int meter_y = row_y + (ui_track_item_height / 2) + 4;
    m_meter_proxy->setPos(meter_x, meter_y);
    m_meter_proxy->setZValue(2);

    updateMuteButton();
    updateSoloButton();

    connect(m_mute_button, &QPushButton::clicked, this, [this]() {
        m_muted = m_mute_button->isChecked();
        updateMuteButton();
        update();
        emit muteToggled(m_row, m_muted);
    });

    connect(m_solo_button, &QPushButton::clicked, this, [this]() {
        bool soloed = m_solo_button->isChecked();
        updateSoloButton();
        update();
        emit soloToggled(m_row, soloed);
    });
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
    m_playing_voice_type = voiceType;
    update();  // trigger repaint
}

void GraphicsTrackItem::clearPlayingInfo() {
    m_playing_voice_type.clear();
    update();
}

bool GraphicsTrackItem::isSoloed() const {
    return m_solo_button && m_solo_button->isChecked();
}

void GraphicsTrackItem::setMuted(bool muted) {
    if (!m_mute_button) return;
    QSignalBlocker blocker(m_mute_button);
    m_muted = muted;
    m_mute_button->setChecked(muted);
    updateMuteButton();
    update();
}

void GraphicsTrackItem::setSoloed(bool soloed) {
    if (!m_solo_button) return;
    QSignalBlocker blocker(m_solo_button);
    m_solo_button->setChecked(soloed);
    updateSoloButton();
    update();
}

void GraphicsTrackItem::setMeterLevels(float left, float right) {
    if (m_meter_widget) {
        m_meter_widget->setLevels(left, right);
    }
}

void GraphicsTrackItem::updateMuteButton() {
    if (!m_mute_button) return;

    m_mute_button->setIcon(QIcon(m_muted ? ":/icons/sound-off.svg" : ":/icons/sound-high.svg"));

    if (m_muted) {
        m_mute_button->setStyleSheet("QPushButton { background: #646464; color: white; border-radius: 3px; }");
    } else {
        const QColor c = m_color_light;
        m_mute_button->setStyleSheet(QString("QPushButton { background: %1; color: black; border-radius: 3px; }")
                                         .arg(c.name()));
    }
}

void GraphicsTrackItem::updateSoloButton() {
    if (!m_solo_button) return;

    if (m_solo_button->isChecked()) {
        m_solo_button->setStyleSheet("QPushButton { background: #f2c94c; color: black; border-radius: 3px; }");
    } else {
        const QColor c = m_color_light;
        m_solo_button->setStyleSheet(QString("QPushButton { background: %1; color: black; border-radius: 3px; }")
                                         .arg(c.name()));
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
