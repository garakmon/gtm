#include "measureroll.h"

#include <QPainterPath>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>

#include "colors.h"
#include "song.h"



MeasureRoll::MeasureRoll(QObject *parent) : QObject(parent) {
    //
    createPlaybackGuide();
}

bool MeasureRoll::advance() {
    //
    this->m_current_tick += 1;
    return true;
    // check for end of song
}

void MeasureRoll::drawMeasures() {
    // !TODO: this crashes when there are no events in the midifile, so do some checking somewhere.
    this->m_scene_measures.clear();

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
                this->m_scene_measures.addPath(header_box, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 100)));

                auto* m_text = new QGraphicsSimpleTextItem(QString::number(measure_number));
                m_text->setBrush(Qt::white);
                QFont m_font = m_text->font(); m_font.setPixelSize(8); m_font.setBold(true);
                m_text->setFont(m_font);
                
                if (m_text->boundingRect().width() < measure_width) {
                    this->m_scene_measures.addItem(m_text);
                    m_text->setPos(measure_x + (measure_width / 2.0) - (m_text->boundingRect().width() / 2.0), 0);
                }
                else {
                    // don't draw the text if there is not enough room in measure
                    delete m_text;
                }
            }

            // draw [measure.beat] numbers and timestamps in MM:SS.ms for the first beat in each measure
            for (int beat_id = 0; beat_id < current_num; ++beat_id) {
                int beat_tick = measure_start_tick + (beat_id * ticks_per_beat);
                if (beat_tick >= end_of_segment) break;

                int x_pos = beat_tick * ui_tick_x_scale;
                bool is_start = (beat_id == 0);

                // Vertical Lines
                this->m_scene_measures.addLine(x_pos, is_start ? 0 : 10, x_pos, 30, 
                                               QPen(is_start ? QColor(255, 255, 255, 180) : QColor(255, 255, 255, 60), 
                                               1, is_start ? Qt::SolidLine : Qt::DotLine));

                // timestamp
                if (is_start) {
                    double total_seconds = this->m_active_song->getTimeInSeconds(beat_tick);
                    int minutes = static_cast<int>(total_seconds) / 60;
                    int seconds = static_cast<int>(total_seconds) % 60;
                    int millis = static_cast<int>((total_seconds - static_cast<int>(total_seconds)) * 1000);

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
                }

                // beat
                QGraphicsSimpleTextItem *b_item = new QGraphicsSimpleTextItem(QString("%1.%2").arg(measure_number).arg(beat_id + 1));
                b_item->setBrush(is_start ? Qt::white : QColor(200, 200, 200));
                QFont b_font = b_item->font(); b_font.setPixelSize(7);
                b_item->setFont(b_font);
                this->m_scene_measures.addItem(b_item);
                b_item->setPos(x_pos + 4, 21); // bottom 10 pixels
            }

            current_tick = measure_end_tick;
            measure_number++;
        }
        
        if (it_sig != time_sigs.end()) {
            it_sig++;
        }
    }

    this->m_scene_measures.setBackgroundBrush(ui_color_piano_roll_bg);
    this->m_scene_measures.setSceneRect(QRect(0, 0, final_tick * ui_tick_x_scale, ui_measure_roll_height));

    this->createPlaybackGuide();
}

void MeasureRoll::createPlaybackGuide() {
    QPolygonF triangle;
    triangle << QPointF(-5, -5) << QPointF(5, -5) << QPointF(0, 0);

    this->m_playhead_arrow = new QGraphicsPolygonItem(triangle);

    this->m_playhead_arrow->setBrush(QBrush(ui_color_measure_guide));
    this->m_playhead_arrow->setPen(Qt::NoPen);

    this->m_playhead_arrow->setZValue(100);

    this->m_scene_measures.addItem(this->m_playhead_arrow);

    this->m_playhead_arrow->setPos(0, 30);
}

void MeasureRoll::updatePlaybackGuide(int tick) {
    // Calculate X based on your 2px per tick scale
    int x_pos = tick * ui_tick_x_scale;

    // Move the visuals
    if (this->m_playhead_arrow) {
        this->m_playhead_arrow->setPos(x_pos, 30);
    }
}
