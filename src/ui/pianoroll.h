#pragma once

#include <QObject>
#include <QGraphicsScene>
#include <memory>

#include "graphicspianokeyitem.h"



// PianoRoll that displays a song (optional) on a piano-roll style display

// piano <keys, one layer stuck to left side>, score (Scrolling part with score note items)

// class GraphicsSceneScore : public QGraphicsScene { // TODO: move to another file?
//     Q_OBJECT
// public:
//     explicit GraphicsSceneScore(QObject *parent = nullptr) : QGraphicsScene(parent) { }

// protected:
//     void drawBackground(QPainter *painter, const QRectF &rect) override;
// };

class Song;
class GraphicsScoreNoteItem;
namespace smf { class MidiEvent; }

class PianoRoll : public QObject {
    Q_OBJECT
    // QGraphicsScene *scene_score = nullptr;
public:
    PianoRoll(QObject *parent = nullptr);

    QGraphicsScene *scenePiano() { return &this->m_scene_piano; };
    QGraphicsScene *sceneRoll() { return &this->m_scene_roll; };

    QList<GraphicsPianoKeyItem *> keys() { return this->m_piano_keys; }
    QList<QGraphicsRectItem *> lines() { return this->m_score_lines; }

    void setSong(std::shared_ptr<Song> song) {
        this->m_active_song = song;
        drawScoreNotes();
    }

    // MidiEvent *
    GraphicsScoreNoteItem *addNote(int track, smf::MidiEvent *event);

    // void resize(); // resize m_score_lines

    void display() {
        this->drawScoreArea();
        this->drawScoreNotes();
    }

signals:
    void eventItemSelected(smf::MidiEvent *event);

private:
    void drawPiano();
    void drawScoreArea();
    void drawScoreNotes();

private:
    // QList<ScoreNoteItem *>
    // vert scrolls the same
    QGraphicsScene m_scene_piano;
    QGraphicsScene m_scene_roll;

    QList<GraphicsPianoKeyItem *> m_piano_keys;
    QList<QGraphicsRectItem *> m_score_lines;

    std::shared_ptr<Song> m_active_song;
};
