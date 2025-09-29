#include "pianoroll.h"

#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QEvent>
#include <QWheelEvent>

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
        //item->setPos(0, note_to_y_pos(i));
        this->m_piano_keys.append(item);
        this->m_scene_piano.addItem(item);
    }

    QRectF rect = this->m_scene_piano.itemsBoundingRect();
    this->m_scene_piano.setSceneRect(rect);
}

void PianoRoll::drawScoreArea() {
    // Draw horizontal lines for piano keys
    int final_tick = this->m_active_song->durationInTicks();
    for (int i = 0; i < g_num_notes_piano; i++) {
        QGraphicsRectItem *item;
        // TODO: calculate height of band, for each octave reset
        int y = scoreNotePosition(i).y;
        int height = scoreNotePosition(i).height;
        if (isNoteWhite(i)) {
            item = new QGraphicsRectItem(0, y, final_tick * ui_tick_x_scale, height);
            item->setBrush(ui_color_score_line_light);
            item->setPen(ui_color_score_line_dark);
            item->setZValue(-1);
        } else {
            item = new QGraphicsRectItem(0, y, final_tick * ui_tick_x_scale, height);
            item->setBrush(ui_color_score_line_dark);
            item->setPen(ui_color_score_line_light);
            item->setZValue(-1);
        }
        this->m_score_lines.append(item);
        this->m_scene_roll.addItem(item);
    }

    // Draw vertical tick marks every quarter note
    static QPen tick_pen(ui_color_piano_roll_tick_mark, 0);//ui_color_piano_roll_tick_mark);
    for (int i = 0; i < final_tick; i += this->m_active_song->getTicksPerQuarterNote()) {
        int x = i * ui_tick_x_scale;
        QGraphicsLineItem *line = new QGraphicsLineItem(x, 0, x, ui_score_line_height * g_num_notes_piano);
        line->setPen(tick_pen);
        line->setZValue(-1);
        this->m_scene_roll.addItem(line);
    }
}

void PianoRoll::drawScoreNotes() {
    // TODO: how to get trackitems from mainwindow?
    // remove
    
}

GraphicsScoreNoteItem *PianoRoll::addNote(int track, smf::MidiEvent *event) {
    GraphicsScoreNoteItem *item = new GraphicsScoreNoteItem(track, event, event->getLinkedEvent());
    m_scene_roll.addItem(item);
    return item;
}
