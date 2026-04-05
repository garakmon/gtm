#include "ui/measureroll.h"

#include "sound/song.h"
#include "ui/colors.h"
#include "ui/graphicsmetaeventitem.h"
#include "util/constants.h"

#include <QApplication>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>



MeasureRollScene::MeasureRollScene(QObject *parent) : QGraphicsScene(parent) {}

void MeasureRollScene::setMeasureRoll(MeasureRoll *measure_roll) {
    m_measure_roll = measure_roll;
}

void MeasureRollScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (m_measure_roll && m_measure_roll->isTimeSelectEnabled()) {
        m_measure_roll->beginTimeSelect(event->scenePos());
        event->accept();
        return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void MeasureRollScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_measure_roll && m_measure_roll->m_time_select.pressed) {
        m_measure_roll->updateTimeSelect(event->scenePos());
        event->accept();
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void MeasureRollScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_measure_roll && m_measure_roll->m_time_select.pressed) {
        m_measure_roll->updateTimeSelect(event->scenePos());
        m_measure_roll->finishTimeSelect();
        event->accept();
        return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
}



MeasureRoll::MeasureRoll(QObject *parent) : QObject(parent) {
    m_scene_measures.setMeasureRoll(this);
    m_time_select_preview.setVisible(false);
    this->createPlaybackGuide();
}

QGraphicsScene *MeasureRoll::sceneMeasures() {
    return &m_scene_measures;
}

void MeasureRoll::setSong(std::shared_ptr<Song> song) {
    m_active_song = song;
    this->drawMeasures();
    this->drawMetaEvents();
}

int MeasureRoll::tick() {
    return m_current_tick;
}

bool MeasureRoll::advance() {
    this->m_current_tick += 1;
    // !TODO: check for end of song?
    return true;
}

void MeasureRoll::setTick(int tick) {
    m_current_tick = tick;
}

void MeasureRoll::setTimeSelectEnabled(bool enabled) {
    m_time_select_enabled = enabled;
    if (!enabled) {
        this->cancelTimeSelect();
    }
}

bool MeasureRoll::isTimeSelectEnabled() const {
    return m_time_select_enabled;
}

void MeasureRoll::resetScene() {
    this->m_scene_measures.clear();
    m_meta_event_items.clear();
    m_playhead_line = nullptr;
    m_playhead_arrow = nullptr;
    m_last_playhead_x = -1;
    this->clearTimeSelect();
}

void MeasureRoll::drawMeasures() {
    // !TODO: this crashes when there are no events in the midifile,
    //        so do some checking somewhere.
    this->resetScene();

    if (!this->m_active_song) {
        return;
    }

    int tpqn = this->m_active_song->getTicksPerQuarterNote();
    auto &time_sigs = this->m_active_song->getTimeSignatures();
    int final_tick = this->m_active_song->durationInTicks();

    int measure_number = 1; // 1-index the measures
    int current_tick = 0;

    int current_num = 4; // default is 4/4
    int current_den = 4;

    auto it_sig = time_sigs.begin();

    // check for start-of-song signature
    if (it_sig != time_sigs.end() && it_sig.key() == 0) {
        smf::MidiEvent* first_sig = it_sig.value();
        current_num = (*first_sig)[3];
        current_den = static_cast<int>(std::pow(2, (*first_sig)[4]));
    }

    while (current_tick < final_tick) {
        if (it_sig != time_sigs.end() && it_sig.key() <= current_tick) {
            smf::MidiEvent* sig_event = it_sig.value();
            current_num = (*sig_event)[3]; 
            current_den = static_cast<int>(std::pow(2, (*sig_event)[4]));
        }

        int ticks_per_beat = (tpqn * 4 / current_den);
        int ticks_per_measure = current_num * ticks_per_beat;
        
        auto next_it = std::next(it_sig);
        int end_of_segment = (next_it != time_sigs.end()) ? next_it.key() : final_tick;

        while (current_tick < end_of_segment) {
            int measure_start_tick = current_tick;
            int measure_end_tick = std::min(current_tick + ticks_per_measure, end_of_segment);
            
            int measure_x = measure_start_tick * ui_tick_x_scale;
            int measure_width = (measure_end_tick * ui_tick_x_scale) - measure_x;

            // measure header number in large rounded box spanning full measure
            if (measure_width > 0) {
                QPainterPath header_box;
                header_box.addRoundedRect(measure_x + 1, 1, measure_width - 2, 8, 3, 3);
                auto *header_item = this->m_scene_measures.addPath(
                    header_box, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 100))
                );
                header_item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

                auto* m_text = new QGraphicsSimpleTextItem(QString::number(measure_number));
                m_text->setBrush(Qt::white);
                QFont m_font = m_text->font(); m_font.setPixelSize(8); m_font.setBold(true);
                m_text->setFont(m_font);
                
                if (m_text->boundingRect().width() < measure_width) {
                    this->m_scene_measures.addItem(m_text);
                    m_text->setPos(measure_x + (measure_width / 2.0)
                                   - (m_text->boundingRect().width() / 2.0), 0);
                    m_text->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
                }
                else {
                    // don't draw the text if there is not enough room in measure
                    delete m_text;
                }
            }

            // draw [measure.beat] numbers and timestamps in MM:SS.ms
            // for the first beat in each measure
            for (int beat_id = 0; beat_id < current_num; ++beat_id) {
                int beat_tick = measure_start_tick + (beat_id * ticks_per_beat);
                if (beat_tick >= end_of_segment) break;

                int x_pos = beat_tick * ui_tick_x_scale;
                bool is_start = (beat_id == 0);

                // vertical Lines
                auto *line_item = this->m_scene_measures.addLine(
                    x_pos, is_start ? 0 : 10, x_pos, ui_measure_roll_height,
                    QPen(is_start ? QColor(255, 255, 255, 180) : QColor(255, 255, 255, 60),
                    1, is_start ? Qt::SolidLine : Qt::DotLine));
                line_item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

                // timestamp
                if (is_start) {
                    double total_seconds = this->m_active_song->getTimeInSeconds(beat_tick);
                    int minutes = static_cast<int>(total_seconds) / 60;
                    int seconds = static_cast<int>(total_seconds) % 60;
                    int millis = static_cast<int>((total_seconds
                                 - static_cast<int>(total_seconds)) * 1000);

                    // MM:SS.ms
                    QString time_string = QString("%1:%2.%3")
                                          .arg(minutes, 2, 10, QChar('0'))
                                          .arg(seconds, 2, 10, QChar('0'))
                                          .arg(millis, 3, 10, QChar('0'));

                    QGraphicsSimpleTextItem *t_item = new QGraphicsSimpleTextItem(time_string);
                    t_item->setBrush(ui_color_measure_timestamps);
                    QFont t_font = t_item->font(); t_font.setPixelSize(7);
                    t_item->setFont(t_font);
                    this->m_scene_measures.addItem(t_item);
                    t_item->setPos(x_pos + 4, 11); // middle 10 pixels
                    t_item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
                }

                // beat
                QGraphicsSimpleTextItem *b_item = new QGraphicsSimpleTextItem(
                    QString("%1.%2").arg(measure_number).arg(beat_id + 1)
                );
                b_item->setBrush(is_start ? Qt::white : QColor(200, 200, 200));
                QFont b_font = b_item->font(); b_font.setPixelSize(7);
                b_item->setFont(b_font);
                this->m_scene_measures.addItem(b_item);
                b_item->setPos(x_pos + 4, 21); // bottom 10 pixels
                b_item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
            }

            current_tick = measure_end_tick;
            measure_number++;
        }
        
        if (it_sig != time_sigs.end()) {
            it_sig++;
        }
    }

    this->m_scene_measures.setBackgroundBrush(ui_color_piano_roll_bg);
    this->m_scene_measures.setSceneRect(QRect(0, 0, final_tick * ui_tick_x_scale,
                                              ui_measure_roll_height));

    this->createPlaybackGuide();
}

void MeasureRoll::createPlaybackGuide() {
    QPolygonF triangle;
    triangle << QPointF(-5, -5) << QPointF(5, -5) << QPointF(0, 0);

    this->m_playhead_arrow = new QGraphicsPolygonItem(triangle);

    this->m_playhead_arrow->setBrush(QBrush(ui_color_measure_guide));
    this->m_playhead_arrow->setPen(Qt::NoPen);
    this->m_playhead_arrow->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    this->m_playhead_arrow->setZValue(100);

    this->m_scene_measures.addItem(this->m_playhead_arrow);

    this->m_playhead_arrow->setPos(0, ui_measure_info_height);
    m_last_playhead_x = -1;
}

void MeasureRoll::updatePlaybackGuide(int tick) {
    int x_pos = tick * ui_tick_x_scale;

    if (this->m_playhead_arrow) {
        if (m_last_playhead_x == x_pos) return;
        m_last_playhead_x = x_pos;
        this->m_playhead_arrow->setPos(x_pos, ui_measure_info_height);
    }
}

void MeasureRoll::drawMetaEvents() {
    for (GraphicsMetaEventItem *item : m_meta_event_items) {
        if (!item) {
            continue;
        }
        if (item->scene() == &m_scene_measures) {
            m_scene_measures.removeItem(item);
        }
        delete item;
    }
    m_meta_event_items.clear();

    if (!m_active_song) {
        return;
    }

    for (auto it = m_active_song->getTimeSignatures().begin();
         it != m_active_song->getTimeSignatures().end();
         it++) {
        auto *item = new GraphicsMetaEventItem(it.value(), GraphicsMetaEventItem::MetaType::TimeSignature);
        m_scene_measures.addItem(item);
        m_meta_event_items.append(item);
    }

    for (auto it = m_active_song->getTempoChanges().begin();
        it != m_active_song->getTempoChanges().end();
        it++) {
        auto *item = new GraphicsMetaEventItem(it.value(), GraphicsMetaEventItem::MetaType::Tempo);
        m_scene_measures.addItem(item);
        m_meta_event_items.append(item);
    }

    for (auto it = m_active_song->getKeySignatures().begin();
        it != m_active_song->getKeySignatures().end();
        it++) {
        auto *item = new GraphicsMetaEventItem(it.value(), GraphicsMetaEventItem::MetaType::KeySignature);
        m_scene_measures.addItem(item);
        m_meta_event_items.append(item);
    }

    for (auto *event : m_active_song->getMarkers()) {
        auto *item = new GraphicsMetaEventItem(event, GraphicsMetaEventItem::MetaType::Marker);
        m_scene_measures.addItem(item);
        m_meta_event_items.append(item);
    }
}

/**
 * Begin a time-range selection drag in the measure roll.
 * The preview stays hidden until the drag threshold is crossed.
 */
void MeasureRoll::beginTimeSelect(const QPointF &scene_pos) {
    this->clearTimeSelect();

    m_time_select.pressed = true;
    m_time_select.dragging = false;
    m_time_select.start_scene_x = scene_pos.x();
    m_time_select.current_scene_x = scene_pos.x();
    m_time_select.behavior = selectionBehaviorFromModifiers(QApplication::keyboardModifiers());
    m_time_select_preview.setRange(scene_pos.x(), scene_pos.x(), ui_measure_info_height);
    m_time_select_preview.setVisible(false);
    m_scene_measures.addItem(&m_time_select_preview);
}

/**
 * Update the time-range preview from the current mouse x position.
 * Only the horizontal span matters, and the preview always uses the header height.
 */
void MeasureRoll::updateTimeSelect(const QPointF &scene_pos) {
    if (!m_time_select.pressed) {
        return;
    }

    m_time_select.current_scene_x = scene_pos.x();
    if (!m_time_select.dragging) {
        const qreal delta = std::abs(scene_pos.x() - m_time_select.start_scene_x);
        if (delta < QApplication::startDragDistance()) {
            return;
        }

        m_time_select.dragging = true;
        m_time_select_preview.setVisible(true);
    }

    m_time_select_preview.setRange(
        m_time_select.start_scene_x, m_time_select.current_scene_x, ui_measure_info_height
    );
}

/**
 * Finish the time-range selection gesture.
 * A click clears selection, while a drag selects notes intersecting the chosen tick span.
 */
void MeasureRoll::finishTimeSelect() {
    if (!m_time_select.pressed) {
        return;
    }

    if (!m_time_select.dragging) {
        if (m_time_select.behavior == SelectionBehavior::Replace) {
            emit onTimeSelectionCleared();
        }
        this->clearTimeSelect();
        return;
    }

    QPair<int, int> tick_range = this->currentTimeSelectTickRange();
    bool modify = (m_time_select.behavior == SelectionBehavior::Modify);
    this->clearTimeSelect();
    emit onTimeRangeSelected(tick_range.first, tick_range.second, modify);
}

void MeasureRoll::cancelTimeSelect() {
    this->clearTimeSelect();
}

/**
 * Clear the temporary time-range selection state and remove its preview item.
 */
void MeasureRoll::clearTimeSelect() {
    if (m_time_select_preview.scene() == &m_scene_measures) {
        m_scene_measures.removeItem(&m_time_select_preview);
    }
    m_time_select_preview.setVisible(false);

    m_time_select.pressed = false;
    m_time_select.dragging = false;
    m_time_select.start_scene_x = 0.0;
    m_time_select.current_scene_x = 0.0;
}

/**
 * Convert the current dragged x span into a normalized tick range.
 */
QPair<int, int> MeasureRoll::currentTimeSelectTickRange() const {
    const qreal left = std::min(m_time_select.start_scene_x, m_time_select.current_scene_x);
    const qreal right = std::max(m_time_select.start_scene_x, m_time_select.current_scene_x);
    const int start_tick = static_cast<int>(left / ui_tick_x_scale);
    const int end_tick = static_cast<int>(right / ui_tick_x_scale);
    return QPair<int, int>(start_tick, end_tick);
}
