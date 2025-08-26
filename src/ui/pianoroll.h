#pragma once

#include <QObject>
#include <QGraphicsScene>

#include "graphicspianokeyitem.h"



// PianoRoll that displays a song (optional) on a piano-roll style display

// piano <keys, one layer stuck to left side>, score (Scrolling part with score note items)
class PianoRoll : public QObject {
    Q_OBJECT
    // QGraphicsScene *scene_score = nullptr;
public:
    PianoRoll(QObject *parent = nullptr);

    QGraphicsScene *scenePiano() { return &this->m_scene_piano; };
    QGraphicsScene *sceneRoll() { return &this->m_scene_roll; };

    QList<GraphicsPianoKeyItem *> keys() { return this->m_piano_keys; }

private:
    void drawPiano();
    // drawScoreArea();

private:
    // QList<ScoreNoteItem *>
    // vert scrolls the same
    QGraphicsScene m_scene_piano;
    QGraphicsScene m_scene_roll;

    QList<GraphicsPianoKeyItem *> m_piano_keys;
};
