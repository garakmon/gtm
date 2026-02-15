#include "ui/graphicstrackitem.h"

#include <QPainter>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>

#include "ui/colors.h"
#include "ui/graphicsscorenoteitem.h"
#include "ui/trackbuttonitem.h"
#include "ui/trackmeteritem.h"
#include "sound/soundtypes.h"
#include "util/constants.h"
#include "deps/midifile/MidiEvent.h"
#include "deps/midifile/MidiEventList.h"

namespace {
TrackEventViewMask viewFlagForType(GraphicsTrackMetaEventItem::EventType type) {
    switch (type) {
    case GraphicsTrackMetaEventItem::EventType::Program: return TrackEventView_Program;
    case GraphicsTrackMetaEventItem::EventType::Volume: return TrackEventView_Volume;
    case GraphicsTrackMetaEventItem::EventType::Expression: return TrackEventView_Expression;
    case GraphicsTrackMetaEventItem::EventType::Pan: return TrackEventView_Pan;
    case GraphicsTrackMetaEventItem::EventType::Pitch: return TrackEventView_Pitch;
    case GraphicsTrackMetaEventItem::EventType::ControlOther:
    default:
        return TrackEventView_ControlOther;
    }
}

QString instrumentTypeAbbrev(const Instrument *inst) {
    if (!inst) return "-";

    static const QMap<int, QString> s_base_type = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> s_duty_map = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    if (s_base_type.contains(inst->type_id)) {
        return s_base_type.value(inst->type_id);
    }
    switch (inst->type_id) {
    case 0x01:
    case 0x09:
        return QString("Sq.%1S").arg(s_duty_map.value(inst->duty_cycle & 0x03, "50"));
    case 0x02:
    case 0x0A:
        return QString("Sq.%1").arg(s_duty_map.value(inst->duty_cycle & 0x03, "50"));
    case 0x04:
    case 0x0C:
        return (inst->duty_cycle & 0x1) ? "Ns.7" : "Ns.15";
    case 0x40:
        return "Split";
    case 0x80:
        return "Drum";
    default:
        return "-";
    }
}
} // namespace



GraphicsTrackItem::GraphicsTrackItem(int track, int row, QGraphicsItem *parent) : QGraphicsObject(parent) {
    this->m_track = track;
    this->m_row = row;
    int color_index = row < g_max_num_tracks ? row : g_max_num_tracks - 1;
    this->m_color = ui_track_color_array[color_index];
    this->m_color_light = ui_track_color_array[color_index].lighter(150);

    m_y_position = ui_track_item_height * this->m_row;
    const int row_y = m_y_position;
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

    // Centered stereo meter
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
    int h = ui_track_item_height + (m_expanded ? ui_automation_total_height : 0);
    return QRectF(0, m_y_position, ui_track_item_width, h);
}

void GraphicsTrackItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QRectF rect = this->boundingRect();
    const qreal header_y = rect.y();
    const qreal header_h = ui_track_item_height;

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

        const qreal text_top = header_y + 1.0;
        const qreal text_h = (header_h * 0.45) - 1.0;
        QRectF textRect(meter_x, text_top, meter_w, text_h);
        painter->drawText(textRect, Qt::AlignCenter, info);
    }

    // Draw expanded automation sub-lane labels
    if (m_expanded) {
        const QString labels[] = {"Volume", "Expression", "Pan", "Pitch Bend"};
        const Qt::PenStyle styles[] = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
        const QColor label_color = Qt::white;
        const int base_y = m_y_position + ui_track_item_height;
        for (int i = 0; i < ui_automation_lane_count; i++) {
            int lane_y = base_y + (i * ui_automation_lane_height);
            QRectF lane_rect(0, lane_y, ui_track_item_width, ui_automation_lane_height);

            // Alternating darker background
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_color.darker(i % 2 == 0 ? 140 : 150));
            painter->drawRect(lane_rect);

            // Line-style sample (12px segment)
            qreal sample_x = 4.0;
            qreal sample_y = lane_y + ui_automation_lane_height / 2.0;
            qreal sample_w = (styles[i] == Qt::SolidLine) ? 1.5 : 1.0;
            painter->setPen(QPen(label_color, sample_w, styles[i]));
            painter->drawLine(QPointF(sample_x, sample_y), QPointF(sample_x + 12.0, sample_y));

            // Label
            painter->setPen(label_color);
            painter->setFont(QFont("IBM Plex Sans", 7));
            painter->drawText(lane_rect.adjusted(20, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, labels[i]);
        }
    }
}

void GraphicsTrackItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event && event->button() == Qt::LeftButton) {
        emit trackClicked(m_track);
        event->accept();
        return;
    }
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

void GraphicsTrackItem::setControlsEnabled(bool enabled) {
    if (m_mute_button) {
        m_mute_button->setEnabled(enabled);
        m_mute_button->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
    }
    if (m_solo_button) {
        m_solo_button->setEnabled(enabled);
        m_solo_button->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
    }
}

void GraphicsTrackItem::setYPosition(int y) {
    if (m_y_position == y) return;
    prepareGeometryChange();
    int dy = y - m_y_position;
    m_y_position = y;
    // Reposition child items by the delta
    if (m_mute_button) m_mute_button->moveBy(0, dy);
    if (m_solo_button) m_solo_button->moveBy(0, dy);
    if (m_meter_item) m_meter_item->moveBy(0, dy);
    update();
}

void GraphicsTrackItem::setExpanded(bool expanded) {
    if (m_expanded == expanded) return;
    prepareGeometryChange();
    m_expanded = expanded;
    update();
}

int GraphicsTrackItem::totalHeight() const {
    return ui_track_item_height + (m_expanded ? ui_automation_total_height : 0);
}



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackMetaEventItem::GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track,
                                                       const VoiceGroup *song_voicegroup,
                                                       const QMap<QString, VoiceGroup> *all_voicegroups,
                                                       const QMap<QString, KeysplitTable> *keysplit_tables)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event = event;
    this->m_song_voicegroup = song_voicegroup;
    this->m_all_voicegroups = all_voicegroups;
    this->m_keysplit_tables = keysplit_tables;
    m_type = detectType(event);
    if (m_type == EventType::Program) {
        m_program_hover_text = buildProgramHoverText();
    }

    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
    setZValue(1.0);
}

QRectF GraphicsTrackMetaEventItem::boundingRect() const {
    QFont hover_font("IBM Plex Mono", 0);
    hover_font.setPixelSize(9);
    hover_font.setStyleHint(QFont::TypeWriter);
    hover_font.setFixedPitch(true);
    const QFontMetrics hover_fm(hover_font);
    const int text_w = qMax(0, hover_fm.horizontalAdvance(hoverText()) + 18);
    const qreal width = qMax<qreal>(84.0, static_cast<qreal>(text_w));

    if (m_type == EventType::Program) {
        // Full-height vertical line from top of track to bottom
        int track_h = ui_track_item_height;
        if (m_parent_track && m_parent_track->isExpanded())
            track_h = m_parent_track->totalHeight();
        qreal line_top = -(ui_track_item_height - 3.0);
        return QRectF(-6.0, line_top, width, track_h + 6.0);
    }
    // Keep a generous local rect for hover chip and minimum hit target.
    return QRectF(-6.0, -20.0, width, 32.0);
}

QPainterPath GraphicsTrackMetaEventItem::shape() const {
    // 12x12 hit target centered on the marker x.
    QPainterPath p;
    p.addRect(QRectF(-6.0, -3.0, 12.0, 12.0));
    return p;
}

GraphicsTrackMetaEventItem::EventType GraphicsTrackMetaEventItem::detectType(const smf::MidiEvent *event) {
    if (!event) return EventType::ControlOther;
    if (event->isPatchChange()) return EventType::Program;
    if (event->isPitchbend()) return EventType::Pitch;
    if (!event->isController()) return EventType::ControlOther;

    const int cc = event->getP1();
    if (cc == 7) return EventType::Volume;
    if (cc == 10) return EventType::Pan;
    if (cc == 11) return EventType::Expression;
    return EventType::ControlOther;
}

QColor GraphicsTrackMetaEventItem::eventColor() const {
    switch (m_type) {
    case EventType::Program: return ui_event_program;
    case EventType::Volume: return ui_event_volume;
    case EventType::Pan: return ui_event_pan;
    case EventType::Expression: return ui_event_expression;
    case EventType::Pitch: return ui_event_pitch;
    case EventType::ControlOther:
    default:
        return ui_event_control_other;
    }
}

QString GraphicsTrackMetaEventItem::hoverText() const {
    if (!m_event) return QString();
    if (m_type == EventType::Program && !m_program_hover_text.isEmpty()) {
        return m_program_hover_text;
    }
    if (m_event->isPatchChange()) {
        return QString("Prog %1").arg(m_event->getP1());
    }
    if (m_event->isController()) {
        const int cc = m_event->getP1();
        const int val = m_event->getP2();
        if (cc == 7) return QString("Vol %1").arg(val);
        if (cc == 10) return QString("Pan %1").arg(val);
        if (cc == 11) return QString("Expr %1").arg(val);
        return QString("CC%1 %2").arg(cc).arg(val);
    }
    if (m_event->isPitchbend()) {
        const int bend = (m_event->getP2() << 7) | m_event->getP1();
        return QString("Bend %1").arg(bend - 8192);
    }
    return QString();
}

const Instrument *GraphicsTrackMetaEventItem::resolveInstrument(const Instrument *inst, uint8_t key) const {
    if (!inst || !m_all_voicegroups) return inst;

    if (inst->type_id == 0x40) { // voice_keysplit
        auto vg_it = m_all_voicegroups->find(inst->sample_label);
        if (vg_it == m_all_voicegroups->end()) return nullptr;

        int inst_index = 0;
        if (m_keysplit_tables && !inst->keysplit_table.isEmpty()) {
            auto table_it = m_keysplit_tables->find(inst->keysplit_table);
            if (table_it != m_keysplit_tables->end()) {
                const KeysplitTable &table = table_it.value();
                auto note_it = table.note_map.find(key);
                if (note_it != table.note_map.end()) {
                    inst_index = note_it.value();
                } else {
                    for (auto it = table.note_map.begin(); it != table.note_map.end(); ++it) {
                        if (it.key() <= key) {
                            inst_index = it.value();
                        }
                    }
                }
            }
        }

        const VoiceGroup &split_vg = vg_it.value();
        if (inst_index < 0 || inst_index >= split_vg.instruments.size()) return nullptr;
        return resolveInstrument(&split_vg.instruments[inst_index], key);
    }

    if (inst->type_id == 0x80) { // voice_keysplit_all
        auto it = m_all_voicegroups->find(inst->sample_label);
        if (it == m_all_voicegroups->end()) return nullptr;
        const VoiceGroup &drum_vg = it.value();
        const int drum_index = key - drum_vg.offset;
        if (drum_index < 0 || drum_index >= drum_vg.instruments.size()) return nullptr;
        return resolveInstrument(&drum_vg.instruments[drum_index], key);
    }

    return inst;
}

QString GraphicsTrackMetaEventItem::buildProgramHoverText() const {
    if (!m_event) return QString();
    const int program = m_event->getP1();
    if (!m_song_voicegroup) {
        return QString("Prog %1").arg(program);
    }
    if (program < 0 || program >= m_song_voicegroup->instruments.size()) {
        return QString("Prog %1").arg(program);
    }

    const Instrument *inst = &m_song_voicegroup->instruments[program];
    const Instrument *resolved = resolveInstrument(inst, static_cast<uint8_t>(inst->base_key));
    const Instrument *display = resolved ? resolved : inst;

    const QString type = instrumentTypeAbbrev(display);
    QString source;
    if (display->type_id == 0x40 || display->type_id == 0x80) {
        source = display->sample_label;
    } else if (!display->sample_label.isEmpty()) {
        source = display->sample_label;
    }

    if (!source.isEmpty()) {
        return QString("P%1 %2 %3").arg(program).arg(type).arg(source);
    }
    return QString("P%1 %2").arg(program).arg(type);
}

void GraphicsTrackMetaEventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    constexpr qreal k_strip_top = 0.0;
    constexpr qreal k_marker_h = 7.0;
    constexpr qreal k_marker_w = 2.5;
    constexpr qreal k_hover_marker_w = 4.0;
    constexpr qreal k_hover_marker_h = 9.0;

    const bool selected = isSelected();
    const QColor color = eventColor();
    const qreal marker_w = (m_hovered || selected) ? k_hover_marker_w : k_marker_w;
    const qreal marker_h = (m_hovered || selected) ? k_hover_marker_h : k_marker_h;
    const QRectF marker_rect(-marker_w * 0.5, k_strip_top + 1.0, marker_w, marker_h);

    painter->setRenderHint(QPainter::Antialiasing, false);

    if (m_type == EventType::Program) {
        // Full-height vertical divider line for program changes
        int track_h = ui_track_item_height;
        if (m_parent_track && m_parent_track->isExpanded())
            track_h = m_parent_track->totalHeight();
        // y position is relative to our placement at lane_base_y
        const qreal line_top = -(ui_track_item_height - 3.0);
        const qreal line_bot = line_top + track_h;
        QColor prog_color = m_parent_track ? m_parent_track->color().darker(160) : color;
        painter->setPen(QPen(prog_color, (m_hovered || selected) ? 1.5 : 0.75, Qt::DashLine));
        painter->drawLine(QPointF(0.0, line_top), QPointF(0.0, line_bot));

        // Small label chip at top
        QString label = QString("P%1").arg(m_event ? m_event->getP1() : 0);
        QFont f("IBM Plex Mono", 0);
        f.setPixelSize(7);
        f.setStyleHint(QFont::TypeWriter);
        f.setFixedPitch(true);
        painter->setFont(f);
        QFontMetrics fm(f);
        int w = fm.horizontalAdvance(label) + 4;
        QRectF chip(2.0, line_top, w, 9.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(prog_color);
        painter->drawRoundedRect(chip, 2, 2);
        painter->setPen(m_parent_track ? m_parent_track->colorLight() : Qt::white);
        painter->drawText(chip, Qt::AlignCenter, label);
    } else {
        painter->setPen(QPen(color, marker_w, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(QPointF(0.0, k_strip_top + 1.0), QPointF(0.0, k_strip_top + 1.0 + marker_h));
    }

    if (selected) {
        painter->setPen(QPen(ui_color_score_line_light, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(marker_rect.adjusted(-2.0, -1.0, 2.0, 1.0));
    }

    // Collision indicator
    if (m_bucket_count >= 2 && m_bucket_count <= 3 && m_bucket_overflow == 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color.lighter(130));
        painter->drawRect(QRectF(-1.0, k_strip_top, 2.0, 1.0));
    } else if (m_bucket_overflow > 0) {
        const QString badge = QString("+%1").arg(m_bucket_overflow);
        QFont f = painter->font();
        f.setFamily("IBM Plex Mono");
        f.setPixelSize(8);
        f.setStyleHint(QFont::TypeWriter);
        f.setFixedPitch(true);
        painter->setFont(f);
        const QFontMetrics fm(f);
        const int w = fm.horizontalAdvance(badge) + 6;
        const QRectF chip(4.0, -12.0, w, 10.0);
        painter->setPen(QPen(ui_color_score_line_dark));
        painter->setBrush(color.darker(125));
        painter->drawRoundedRect(chip, 3, 3);
        painter->setPen(ui_color_score_line_light);
        painter->drawText(chip, Qt::AlignCenter, badge);
    }

    if (m_hovered || selected) {
        const QString text = hoverText();
        if (!text.isEmpty()) {
            QFont f = painter->font();
            f.setFamily("IBM Plex Mono");
            f.setPixelSize(9);
            f.setStyleHint(QFont::TypeWriter);
            f.setFixedPitch(true);
            painter->setFont(f);
            const QFontMetrics fm(f);
            const int w = fm.horizontalAdvance(text) + 8;
            const QRectF chip(6.0, -16.0, w, 12.0);
            painter->setPen(QPen(ui_color_score_line_dark));
            painter->setBrush(m_parent_track ? m_parent_track->color().darker(120) : ui_color_score_line_dark);
            painter->drawRoundedRect(chip, 3, 3);
            painter->setPen(ui_color_score_line_light);
            painter->drawText(chip, Qt::AlignCenter, text);
        }
    }
}

void GraphicsTrackMetaEventItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    Q_UNUSED(event);
    m_hovered = true;
    setZValue(3.0);
    update();
}

void GraphicsTrackMetaEventItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    Q_UNUSED(event);
    m_hovered = false;
    setZValue(1.0);
    update();
}

void GraphicsTrackMetaEventItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event && event->button() == Qt::LeftButton) {
        setSelected(!isSelected());
        update();
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event);
}



//////////////////////////////////////////////////////////////////////////////////////////



GraphicsTrackRollManager::GraphicsTrackRollManager(
    smf::MidiEventList *event_list, GraphicsTrackItem *parent_track,
    int initial_vol, int initial_pan, int initial_expr, int initial_bend,
    const VoiceGroup *song_voicegroup,
    const QMap<QString, VoiceGroup> *all_voicegroups,
    const QMap<QString, KeysplitTable> *keysplit_tables)
  : QGraphicsItem(parent_track) {
    this->m_parent_track = parent_track;
    this->m_event_list = event_list;
    this->m_song_voicegroup = song_voicegroup;
    this->m_all_voicegroups = all_voicegroups;
    this->m_keysplit_tables = keysplit_tables;
    setZValue(0.5);
    if (m_parent_track) {
        setPos(0.0, ui_track_item_height * m_parent_track->row());
    }

    // Calculate width from last event
    m_width = 0;
    if (event_list->size() > 0) {
        m_width = (*event_list)[event_list->size() - 1].tick * ui_tick_x_scale;
    }

    // Create child items for control events (program, CC, pitch bend)
    QMap<qint64, QList<GraphicsTrackMetaEventItem *>> buckets;
    constexpr qreal k_bucket_px = 4.0;

    for (int i = 0; i < event_list->size(); i++) {
        smf::MidiEvent *event = &(*event_list)[i];

        if (event->isNoteOn() || event->isNoteOff()) {
            continue;
        }

        if (event->isPatchChange() || event->isController() || event->isPitchbend()) {
            auto *item = new GraphicsTrackMetaEventItem(event, parent_track,
                                                        m_song_voicegroup,
                                                        m_all_voicegroups,
                                                        m_keysplit_tables);
            item->setParentItem(this);
            const int lane = laneForType(item->eventType());
            item->setLane(lane);
            item->setPos(event->tick * ui_tick_x_scale, markerYForLane(lane));
            m_items.append(item);

            item->setVisible(isVisibleType(item->eventType()));

            const int bucket = static_cast<int>(item->x() / k_bucket_px);
            const qint64 key = (static_cast<qint64>(lane) << 32) | static_cast<quint32>(bucket);
            buckets[key].append(item);
        }
    }

    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        const int count = it.value().size();
        const int overflow = qMax(0, count - 3);
        for (auto *item : it.value()) {
            item->setBucketCount(count);
            item->setBucketOverflow(overflow);
        }
    }

    buildStepGraphData(initial_vol, initial_pan, initial_expr, initial_bend);
}

bool GraphicsTrackRollManager::isVisibleType(GraphicsTrackMetaEventItem::EventType type) const {
    return (m_event_view_mask & viewFlagForType(type)) != 0;
}

int GraphicsTrackRollManager::laneForType(GraphicsTrackMetaEventItem::EventType type) const {
    switch (type) {
    case GraphicsTrackMetaEventItem::EventType::Program:
        return 0;
    case GraphicsTrackMetaEventItem::EventType::Volume:
    case GraphicsTrackMetaEventItem::EventType::Expression:
        return 1;
    case GraphicsTrackMetaEventItem::EventType::Pan:
    case GraphicsTrackMetaEventItem::EventType::Pitch:
    case GraphicsTrackMetaEventItem::EventType::ControlOther:
    default:
        return 2;
    }
}

qreal GraphicsTrackRollManager::markerYForLane(int lane) const {
    const qreal lane_base_y = ui_track_item_height - 3.0;
    const qreal lane_step = 3.0;
    return lane_base_y - (qBound(0, lane, 2) * lane_step);
}

void GraphicsTrackRollManager::buildStepGraphData(int initial_vol, int initial_pan,
                                                   int initial_expr, int initial_bend) {
    m_cc_volume.append({0, initial_vol});
    m_cc_pan.append({0, initial_pan});
    m_cc_expression.append({0, initial_expr});
    m_cc_pitch.append({0, initial_bend});

    for (int i = 0; i < m_event_list->size(); i++) {
        smf::MidiEvent &ev = (*m_event_list)[i];
        if (ev.isController()) {
            int cc = ev.getP1();
            int val = ev.getP2();
            if (cc == 7) m_cc_volume.append({ev.tick, val});
            else if (cc == 10) m_cc_pan.append({ev.tick, val});
            else if (cc == 11) m_cc_expression.append({ev.tick, val});
        } else if (ev.isPitchbend()) {
            int bend = (ev.getP2() << 7) | ev.getP1();
            m_cc_pitch.append({ev.tick, bend});
        }
    }
}

void GraphicsTrackRollManager::setExpanded(bool expanded) {
    if (m_expanded == expanded) return;
    prepareGeometryChange();
    m_expanded = expanded;
    update();
}

void GraphicsTrackRollManager::setEventViewMask(TrackEventViewMask mask) {
    const TrackEventViewMask normalized = (mask == 0) ? kTrackEventView_All : mask;
    if (m_event_view_mask == normalized) return;
    m_event_view_mask = normalized;

    for (auto *item : m_items) {
        if (!item) continue;
        item->setVisible(isVisibleType(item->eventType()));
    }
    update();
}

QRectF GraphicsTrackRollManager::boundingRect() const {
    int h = ui_track_item_height + (m_expanded ? ui_automation_total_height : 0);
    return QRectF(0, 0, m_width, h);
}

void GraphicsTrackRollManager::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget);
    if (!m_parent_track) return;

    // Main track row background
    const QRectF row_rect(0.0, 0.0, m_width, ui_track_item_height);
    painter->setPen(QPen(m_parent_track->color().darker(115)));
    painter->setBrush(QColor::fromHsl(m_parent_track->color().hslHue(),
                                      m_parent_track->color().hslSaturation(),
                                      220));
    painter->drawRect(row_rect);

    // Draw mini step graphs in collapsed mode, expanded lanes when expanded
    if (m_expanded) {
        paintExpandedLanes(painter, option);
    } else {
        paintMiniStepGraphs(painter, option);
    }
}

void GraphicsTrackRollManager::paintStepLine(QPainter *painter, const QVector<CCPoint> &data,
                                              Qt::PenStyle pen_style, const QRectF &rect,
                                              int val_min, int val_max) {
    if (data.isEmpty() || rect.width() <= 0 || rect.height() <= 0) return;

    const qreal range = static_cast<qreal>(val_max - val_min);
    if (range <= 0) return;

    // Build stepped path
    QPainterPath path;
    bool started = false;
    for (int i = 0; i < data.size(); i++) {
        qreal x = data[i].tick * ui_tick_x_scale;
        qreal norm = static_cast<qreal>(data[i].value - val_min) / range;
        qreal y = rect.bottom() - norm * rect.height();

        if (!started) {
            path.moveTo(qMax(x, rect.x()), y);
            started = true;
        } else {
            // Horizontal step then vertical
            path.lineTo(x, path.currentPosition().y());
            path.lineTo(x, y);
        }
    }
    // Extend to end of rect
    if (started) {
        path.lineTo(rect.right(), path.currentPosition().y());
    }

    // Draw the step line (opaque, track-derived dark color)
    QColor line_color = m_parent_track->color().darker(200);
    qreal width = (pen_style == Qt::SolidLine) ? 1.5 : 1.0;
    painter->setPen(QPen(line_color, width, pen_style));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

void GraphicsTrackRollManager::paintMiniStepGraphs(QPainter *painter, const QStyleOptionGraphicsItem *option) {
    // Top 24px: step graphs (1px padding → usable 22px)
    const qreal graph_y = 1.0;
    const qreal graph_h = 22.0;
    // Bottom 6px: event strip for ControlOther markers
    const qreal strip_y = 24.0;
    const qreal strip_h = 6.0;

    // Draw slightly darker background for the event strip
    QColor strip_bg = QColor::fromHsl(m_parent_track->color().hslHue(),
                                      m_parent_track->color().hslSaturation(), 205);
    painter->setPen(Qt::NoPen);
    painter->setBrush(strip_bg);
    painter->drawRect(QRectF(0, strip_y, m_width, strip_h));

    // Clip to exposed rect for performance
    const QRectF exposed = option->exposedRect;
    const QRectF graph_rect(exposed.x(), graph_y, exposed.width(), graph_h);

    painter->save();
    painter->setClipRect(QRectF(0, graph_y, m_width, graph_h));
    painter->setRenderHint(QPainter::Antialiasing, false);

    if (m_event_view_mask & TrackEventView_Volume) {
        paintStepLine(painter, m_cc_volume, Qt::SolidLine, graph_rect, 0, 127);
    }
    if (m_event_view_mask & TrackEventView_Expression) {
        paintStepLine(painter, m_cc_expression, Qt::DashLine, graph_rect, 0, 127);
    }
    if (m_event_view_mask & TrackEventView_Pan) {
        paintStepLine(painter, m_cc_pan, Qt::DotLine, graph_rect, 0, 127);
    }
    if (m_event_view_mask & TrackEventView_Pitch) {
        paintStepLine(painter, m_cc_pitch, Qt::DashDotLine, graph_rect, 0, 16383);
    }

    painter->restore();
}

void GraphicsTrackRollManager::paintExpandedLanes(QPainter *painter, const QStyleOptionGraphicsItem *option) {
    const int base_y = ui_track_item_height;
    const QColor track_color = m_parent_track->color();

    const QVector<CCPoint> *data[] = {&m_cc_volume, &m_cc_expression, &m_cc_pan, &m_cc_pitch};
    const int val_maxes[] = {127, 127, 127, 16383};

    // Clip to exposed rect for performance
    const QRectF exposed = option->exposedRect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < ui_automation_lane_count; i++) {
        int lane_y = base_y + (i * ui_automation_lane_height);
        QRectF lane_rect(0, lane_y, m_width, ui_automation_lane_height);

        // Lane background
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor::fromHsl(track_color.hslHue(),
                                          track_color.hslSaturation(),
                                          i % 2 == 0 ? 210 : 215));
        painter->drawRect(lane_rect);

        // Lane separator line (subtle, lighter than background)
        int sep_l = (i % 2 == 0) ? 200 : 205;
        painter->setPen(QPen(QColor::fromHsl(track_color.hslHue(),
                                             track_color.hslSaturation(), sep_l), 0.5));
        painter->drawLine(QPointF(0, lane_y), QPointF(m_width, lane_y));

        // Step graph within lane (2px padding so max values don't sit on border)
        QRectF graph_rect(exposed.x(), lane_y + 2.0, exposed.width(), ui_automation_lane_height - 4.0);
        painter->setClipRect(QRectF(0, lane_y, m_width, ui_automation_lane_height));
        paintStepLine(painter, *data[i], Qt::SolidLine, graph_rect, 0, val_maxes[i]);
        painter->setClipping(false);
    }

    painter->restore();
}
