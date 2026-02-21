#pragma once
#ifndef PIANOROLL_H
#define PIANOROLL_H

#include "ui/graphicspianokeyitem.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMap>
#include <QObject>

#include <memory>



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

class TrackNoteGroup : public QGraphicsObject {
    Q_OBJECT

public:
    explicit TrackNoteGroup(int track, QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent), m_track(track) {}

    int track() const { return m_track; }
    QRectF boundingRect() const override { return childrenBoundingRect(); }
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {}
    void addNote(class GraphicsScoreNoteItem *note) { m_notes.append(note); }
    const QVector<class GraphicsScoreNoteItem *> &notes() const { return m_notes; }

private:
    int m_track = 0;
    QVector<class GraphicsScoreNoteItem *> m_notes;
};

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
        this->m_score_bg = nullptr;
        this->m_scene_roll.clear();
        this->m_track_note_groups.clear();
        //this->m_scene_piano.clear();
        //this->m_piano_keys.clear();

        this->m_active_song = song;
        //drawScoreNotes();
    }

    // MidiEvent *
    GraphicsScoreNoteItem *addNote(int track, int row, smf::MidiEvent *event);

    // void resize(); // resize m_score_lines

    void display() {
        this->drawScoreArea();
        this->drawScoreNotes();
    }

    void setEditsEnabled(bool enabled);
    void selectNotesForTrack(int track, bool clearOthers = true);

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
    QGraphicsPixmapItem *m_score_bg = nullptr;

    std::shared_ptr<Song> m_active_song;
    QMap<int, TrackNoteGroup *> m_track_note_groups;
    bool m_edits_enabled = true;
};

#endif // PIANOROLL_H
