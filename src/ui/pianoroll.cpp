#include "pianoroll.h"

#include <QGraphicsView>
#include <QEvent>
#include <QWheelEvent>

#include "song.h"
#include "constants.h"
#include "colors.h"
#include "util.h"



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    this->m_scene_piano.setParent(this);
    this->drawPiano();
    this->drawScoreArea();
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
        //item->setPos(0, note_to_y_pos(i));
        this->m_piano_keys.append(item);
        this->m_scene_piano.addItem(item);
    }

    QRectF rect = this->m_scene_piano.itemsBoundingRect();
    this->m_scene_piano.setSceneRect(rect);
}

void PianoRoll::drawScoreArea() {
    // TODO: keep track of grid lines so they can be highlighted upon selection/add new notes etc
    int starting_y = ui_score_line_height * g_num_notes_piano - ui_piano_key_black_height;

    for (int i = 0; i < g_num_notes_piano; i++) {
        QGraphicsRectItem *item;
        // TODO: calculate height of band, for each octave reset
        // scroll_pianoArea
        if (isNoteWhite(i)) {
            item = new QGraphicsRectItem(0, starting_y, 10000, ui_piano_key_black_height);
            item->setBrush(ui_color_score_line_light);
            item->setPen(ui_color_score_line_dark);
            item->setZValue(-1);
        } else {
            item = new QGraphicsRectItem(0, starting_y, 10000, ui_piano_key_black_height);
            item->setBrush(ui_color_score_line_dark);
            item->setPen(ui_color_score_line_light);
            item->setZValue(-1);
        }
        starting_y -= ui_score_line_height;
        //item->setPos(0, note_to_y_pos(i));
        this->m_score_lines.append(item);
        this->m_scene_roll.addItem(item);
    }

    // QRectF rect = this->m_scene_roll.itemsBoundingRect();
    // this->m_scene_roll.setSceneRect(rect);
    //QRectF rect = this->m_scene_piano.itemsBoundingRect();
    //this->m_scene_roll.setSceneRect(QRectF(0, 0, 1000, rect.height()));
}

void PianoRoll::drawScoreNotes() {
    // TODO: how to get trackitems from mainwindow?
    // remove
    this->m_active_song->linkNotePairs();

    double duration;
    for (auto track : this->m_active_song->tracks()) {
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *mev = &(*track)[i];
            if (!mev->isNoteOn()) {
                continue;
            }
            else {
                qDebug() << "event duration:" << mev->getTickDuration();
                int note = mev->getKeyNumber();
                int y = isNoteWhite(note) ? whiteNoteToY(note) : blackNoteToY(note);
                
                this->m_scene_roll.addItem(new QGraphicsRectItem(mev->tick, y, mev->getTickDuration(), ui_piano_key_black_height));
            }
            //duration = mev->getTickDuration();

            // TODO: base color on mev->track
            
        }
    }
}
