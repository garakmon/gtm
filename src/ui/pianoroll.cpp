#include "pianoroll.h"

#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QPainter>
#include <cmath>

#include "graphicsscorenoteitem.h"
#include "song.h"
#include "constants.h"
#include "colors.h"
#include "util.h"



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    this->m_scene_piano.setParent(this);
    this->drawPiano();
    //this->drawScoreArea();
}

void PianoRoll::drawPiano() {
    for (int i = 0; i < g_num_notes_piano; i++) {
        GraphicsPianoKeyItem *item;
        if (isNoteWhite(i)) {
            item = new GraphicsPianoKeyItemWhite(i);
            item->setZValue(0);
            item->setPos(0, whiteNoteToY(i));
        } else {
            item = new GraphicsPianoKeyItemBlack(i);
            item->setZValue(1);
            item->setPos(0, blackNoteToY(i));
        }
        item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        //item->setPos(0, note_to_y_pos(i));
        this->m_piano_keys.append(item);
        this->m_scene_piano.addItem(item);
    }

    QRectF rect = this->m_scene_piano.itemsBoundingRect();
    this->m_scene_piano.setSceneRect(rect);
}

void PianoRoll::drawScoreArea() {
    // Clear
    this->m_score_lines.clear();
    if (m_score_bg) {
        if (m_score_bg->scene() == &m_scene_roll) {
            m_scene_roll.removeItem(m_score_bg);
        }
        delete m_score_bg;
        m_score_bg = nullptr;
    }

    // Draw horizontal lines for piano keys
    int final_tick = this->m_active_song->durationInTicks();
    const int tpqn = this->m_active_song->getTicksPerQuarterNote();
    const int grid_w = qMax(1, final_tick * ui_tick_x_scale);
    const int grid_h = ui_score_line_height * g_num_notes_piano;

    QPixmap bg_pix(grid_w, grid_h);
    bg_pix.fill(ui_color_piano_roll_bg);
    {
        QPainter p(&bg_pix);
        p.setRenderHint(QPainter::Antialiasing, false);

        for (int i = 0; i < g_num_notes_piano; i++) {
            const int y = scoreNotePosition(i).y;
            const int h = scoreNotePosition(i).height;
            QColor fill = isNoteWhite(i) ? ui_color_score_line_light : ui_color_score_line_dark;
            QColor line = isNoteWhite(i) ? ui_color_score_line_dark : ui_color_score_line_light;
            p.fillRect(QRect(0, y, grid_w, h), fill);
            p.setPen(line);
            p.drawLine(0, y + h - 1, grid_w, y + h - 1);
        }

        // Vertical beat lines aligned to measures
        auto &time_sigs = this->m_active_song->getTimeSignatures();
        int current_num = 4;
        int current_den = 4;
        auto it_sig = time_sigs.begin();
        if (it_sig != time_sigs.end() && it_sig.key() == 0) {
            smf::MidiEvent *first_sig = it_sig.value();
            current_num = (*first_sig)[3];
            current_den = static_cast<int>(std::pow(2, (*first_sig)[4]));
        }

        int current_tick = 0;
        while (current_tick < final_tick) {
            if (it_sig != time_sigs.end() && it_sig.key() <= current_tick) {
                smf::MidiEvent *sig_event = it_sig.value();
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

                for (int beat_id = 0; beat_id < current_num; ++beat_id) {
                    int beat_tick = measure_start_tick + (beat_id * ticks_per_beat);
                    if (beat_tick >= end_of_segment) break;

                    int x = beat_tick * ui_tick_x_scale;
                    QColor line = ui_color_piano_roll_tick_mark;
                    if (beat_id == 0) {
                        line.setAlpha(220);
                    } else {
                        line.setAlpha(110);
                    }
                    p.setPen(QPen(line, 0));
                    p.drawLine(x, 0, x, grid_h);
                }

                current_tick = measure_end_tick;
            }

            if (it_sig != time_sigs.end()) {
                it_sig++;
            }
        }
    }

    m_score_bg = m_scene_roll.addPixmap(bg_pix);
    m_score_bg->setZValue(-2);
    m_score_bg->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    m_scene_roll.setBackgroundBrush(Qt::NoBrush);
    m_scene_roll.setSceneRect(0, 0, grid_w, grid_h);
}

void PianoRoll::drawScoreNotes() {
    for (auto [track, note_event] : this->m_active_song->getNotes()) {
        int row = this->m_active_song->getDisplayRow(track);
        this->addNote(track, row, note_event);
    }
}

GraphicsScoreNoteItem *PianoRoll::addNote(int track, int row, smf::MidiEvent *event) {
    TrackNoteGroup *group = nullptr;
    if (m_track_note_groups.contains(track)) {
        group = m_track_note_groups.value(track);
    } else {
        group = new TrackNoteGroup(track);
        group->setPos(0, 0);
        m_scene_roll.addItem(group);
        m_track_note_groups.insert(track, group);
    }

    GraphicsScoreNoteItem *item = new GraphicsScoreNoteItem(this, track, row, event, event->getLinkedEvent());
    item->setParentItem(group);
    group->addNote(item);
    return item;
}

void PianoRoll::setNotesInteractive(bool enabled) {
    for (auto *item : m_scene_roll.items()) {
        if (auto *note = qgraphicsitem_cast<GraphicsScoreNoteItem *>(item)) {
            note->setFlag(QGraphicsItem::ItemIsSelectable, enabled);
            note->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
        }
    }
    m_scene_roll.clearSelection();
    m_scene_roll.invalidate();
    m_scene_roll.update();
}

void PianoRoll::selectNotesForTrack(int track, bool clearOthers) {
    if (clearOthers) {
        m_scene_roll.clearSelection();
    }
    auto it = m_track_note_groups.find(track);
    if (it == m_track_note_groups.end() || !it.value()) {
        return;
    }
    for (auto *note : it.value()->notes()) {
        if (note && note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
            note->setSelected(true);
        }
    }
    m_scene_roll.update();
}
