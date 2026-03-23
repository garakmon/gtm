#pragma once
#ifndef PIANOROLL_H
#define PIANOROLL_H

#include "ui/graphicspianokeyitem.h"
#include "app/structs.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMap>
#include <QObject>
#include <QSet>

#include <memory>



class Song;
class GraphicsScoreNoteItem;
namespace smf { class MidiEvent; }

//////////////////////////////////////////////////////////////////////////////////////////
///
/// TrackNoteGroup is a container for all note items belonging to a single track.
/// It gives the piano roll a stable per-track parent item so notes can be
/// selected and manipulated as a group.
///
//////////////////////////////////////////////////////////////////////////////////////////
class TrackNoteGroup : public QGraphicsObject {
    Q_OBJECT

public:
    explicit TrackNoteGroup(int track, QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent), m_track(track) {}

    ~TrackNoteGroup() override = default;

    TrackNoteGroup(const TrackNoteGroup &) = delete;
    TrackNoteGroup &operator=(const TrackNoteGroup &) = delete;
    TrackNoteGroup(TrackNoteGroup &&) = delete;
    TrackNoteGroup &operator=(TrackNoteGroup &&) = delete;

    // drawing overrides
    QRectF boundingRect() const override { return childrenBoundingRect(); }
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {}

    int track() const { return m_track; }

    // note ownership
    void addNote(GraphicsScoreNoteItem *note) { m_notes.append(note); }
    const QVector<GraphicsScoreNoteItem *> &notes() const { return m_notes; }

private:
    // track state
    int m_track = 0;

    // child items
    QVector<GraphicsScoreNoteItem *> m_notes;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// The PianoRoll owns the piano keys scene, and the main note-roll scene for the editor.
/// It is responsible for building the static piano and roll background graphics, creating
/// note items for the active song, and determining whether those notes are interactible.
///
//////////////////////////////////////////////////////////////////////////////////////////
class PianoRoll : public QObject {
    Q_OBJECT

public:
    PianoRoll(QObject *parent = nullptr);
    ~PianoRoll() override = default;

    // disable copy and move
    PianoRoll(const PianoRoll &) = delete;
    PianoRoll &operator=(const PianoRoll &) = delete;
    PianoRoll(PianoRoll &&) = delete;
    PianoRoll &operator=(PianoRoll &&) = delete;

    // scene access
    QGraphicsScene *scenePiano();
    QGraphicsScene *sceneRoll();

    // item access
    QList<GraphicsPianoKeyItem *> keys();
    QList<QGraphicsRectItem *> lines();

    // song state
    void setSong(std::shared_ptr<Song> song);

    // note display
    GraphicsScoreNoteItem *addNote(int track, int row, smf::MidiEvent *event);
    void display();

    // edit state
    void setEditsEnabled(bool enabled);
    void selectNotesForTrack(int track, bool clearOthers = true);
    void selectEvents(const QVector<smf::MidiEvent *> &events, bool clear_others = true);
    void handleNoteMousePress(
        GraphicsScoreNoteItem *item,
        const QPointF &scene_pos,
        bool resize_start,
        bool resize_end
    );
    void handleNoteMouseMove(GraphicsScoreNoteItem *item, const QPointF &scene_pos);
    void handleNoteMouseRelease(GraphicsScoreNoteItem *item, const QPointF &scene_pos);

signals:
    void eventItemSelected(smf::MidiEvent *event);
    void onSelectedEventsChanged(const QVector<smf::MidiEvent *> &events);
    void onNoteMoveRequested(const NoteMoveSettings &settings);
    void onNoteResizeRequested(const NoteResizeSettings &settings);

private:
    enum class NoteDragMode {
        Move,
        ResizeStart,
        ResizeEnd,
    };

    struct DraggedNoteState {
        GraphicsScoreNoteItem *item = nullptr;
        smf::MidiEvent *note_on = nullptr;
        int start_tick = 0;
        int start_key = 0;
        int start_duration = 0;
        QPointF start_pos;
    };

    struct NoteDragState {
        bool pressed = false;
        bool dragging = false;
        GraphicsScoreNoteItem *anchor_item = nullptr;
        NoteDragMode mode = NoteDragMode::Move;
        QPointF drag_start_scene_pos;
        int anchor_start_tick = 0;
        int anchor_start_key = 0;
        int delta_tick = 0;
        int delta_key = 0;
        QVector<DraggedNoteState> notes;
    };

    // drawing helpers
    void drawPiano();
    void drawScoreArea();
    void drawScoreNotes();
    void beginNoteDrag(GraphicsScoreNoteItem *item, const QPointF &scene_pos);
    void updateNoteDrag(const QPointF &scene_pos);
    void commitNoteDrag();
    void cancelNoteDrag();
    void clearNoteDrag();
    void restoreDraggedNotesToStart();
    void updateSelectedEvents();
    QVector<GraphicsScoreNoteItem *> selectedNoteItems() const;
    int snapTickDelta(double delta_x) const;
    int snapKeyDelta(double delta_y) const;
    int keyForSceneY(double scene_y) const;
    void clampDraggedDelta(int *delta_tick, int *delta_key) const;
    int clampResizeDelta(int delta_tick) const;
    void updateResizePreview();

private:
    // scenes
    QGraphicsScene m_scene_piano;
    QGraphicsScene m_scene_roll;

    // cached items
    QList<GraphicsPianoKeyItem *> m_piano_keys;
    QList<QGraphicsRectItem *> m_score_lines;
    QGraphicsPixmapItem *m_score_bg = nullptr;

    std::shared_ptr<Song> m_active_song;
    QMap<int, TrackNoteGroup *> m_track_note_groups;
    NoteDragState m_note_drag;

    bool m_edits_enabled = true;
    bool m_ignore_selection_updates = false;
};

#endif // PIANOROLL_H
