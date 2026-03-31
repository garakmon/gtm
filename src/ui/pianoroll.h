#pragma once
#ifndef PIANOROLL_H
#define PIANOROLL_H

#include "ui/graphicspianokeyitem.h"
#include "ui/graphicsselectionitems.h"
#include "app/structs.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMap>
#include <QObject>
#include <QSet>

#include <memory>



class Song;
class GraphicsScoreNoteItem;
class GraphicsPreviewNoteItem;
class PianoRoll;
namespace smf { class MidiEvent; }
class QGraphicsSceneMouseEvent;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// PianoRollScene is the graphics scene for the editable note area of the piano roll.
/// It exists so empty-space mouse input can be handled separately from note item input
/// (note items handle moving and resizing existing notes, while the scene handles
/// creating new notes when the user clicks and drags on emptty space).
///
//////////////////////////////////////////////////////////////////////////////////////////
class PianoRollScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit PianoRollScene(QObject *parent = nullptr);
    void setPianoRoll(PianoRoll *piano_roll);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    PianoRoll *m_piano_roll = nullptr;
};

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
    friend class PianoRollScene;

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
    void setCreateNotesEnabled(bool enabled);
    void setDeleteNotesEnabled(bool enabled);
    void setRectSelectEnabled(bool enabled);
    void setLassoSelectEnabled(bool enabled);
    void setActiveTrack(int track);
    bool isDeleteNotesEnabled() const;
    bool isRectSelectEnabled() const;
    bool isLassoSelectEnabled() const;
    void selectNotesForTrack(int track, bool clearOthers = true);
    void selectNotesInTickRange(int start_tick, int end_tick, bool clear_others = true);
    void selectEvents(const QVector<smf::MidiEvent *> &events, bool clear_others = true);
    void handleNoteMousePress(
        GraphicsScoreNoteItem *item,
        const QPointF &scene_pos,
        bool resize_start,
        bool resize_end
    );
    void handleNoteMouseMove(GraphicsScoreNoteItem *item, const QPointF &scene_pos);
    void handleNoteMouseRelease(GraphicsScoreNoteItem *item, const QPointF &scene_pos);
    void handleRectSelectMousePress(const QPointF &scene_pos);
    void handleRectSelectMouseMove(const QPointF &scene_pos);
    void handleRectSelectMouseRelease(const QPointF &scene_pos);
    void handleLassoSelectMousePress(const QPointF &scene_pos);
    void handleLassoSelectMouseMove(const QPointF &scene_pos);
    void handleLassoSelectMouseRelease(const QPointF &scene_pos);

signals:
    void eventItemSelected(smf::MidiEvent *event);
    void onSelectedEventsChanged(const QVector<smf::MidiEvent *> &events);
    void onNoteMoveRequested(const NoteMoveSettings &settings);
    void onNoteResizeRequested(const NoteResizeSettings &settings);
    void onNoteCreateRequested(const QVector<NoteCreateSettings> &notes);
    void onNoteDeleteRequested(const QVector<smf::MidiEvent *> &events);

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

    struct NoteCreateState {
        bool pressed = false;
        bool dragging = false;
        int track = -1;
        int row = 0;
        int channel = 0;
        int velocity = 100;
        QPointF press_scene_pos;
        int anchor_tick = 0;
        int anchor_key = 60;
        int start_tick = 0;
        int duration_ticks = 0;
        GraphicsPreviewNoteItem *preview_item = nullptr;
    };

    struct NoteDeleteState {
        bool pressed = false;
        bool dragging = false;
        QPointF press_scene_pos;
        QSet<smf::MidiEvent *> pending_events;
        QVector<GraphicsScoreNoteItem *> hidden_items;
    };

    struct RectSelectState {
        bool pressed = false;
        bool dragging = false;
        QPointF start_scene_pos;
        QPointF current_scene_pos;
    };

    struct LassoSelectState {
        bool pressed = false;
        bool dragging = false;
        QPointF start_scene_pos;
        QPainterPath path;
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
    int snapTick(int tick) const;
    int snapKeyDelta(double delta_y) const;
    int keyFromSceneY(double scene_y) const;
    void clampDraggedDelta(int *delta_tick, int *delta_key) const;
    int clampResizeDelta(int delta_tick) const;
    void updateResizePreview();
    bool canBeginNoteCreate(const QPointF &scene_pos) const;
    void beginNoteCreate(const QPointF &scene_pos);
    void updateNoteCreate(const QPointF &scene_pos);
    void commitNoteCreate();
    void cancelNoteCreate();
    void clearNoteCreate();
    void updatePreviewNoteGeometry();
    void beginNoteDelete(GraphicsScoreNoteItem *item);
    void updateNoteDelete(const QPointF &scene_pos);
    void commitNoteDelete();
    void cancelNoteDelete();
    void clearNoteDelete(bool restore_visibility);
    void beginRectSelect(const QPointF &scene_pos);
    void updateRectSelect(const QPointF &scene_pos);
    void finishRectSelect();
    void cancelRectSelect();
    void clearRectSelect();
    QRectF currentRectSelectRect() const;
    QVector<GraphicsScoreNoteItem *> noteItemsInRect(const QRectF &rect) const;
    void applyRectSelection(const QVector<GraphicsScoreNoteItem *> &items);
    void beginLassoSelect(const QPointF &scene_pos);
    void updateLassoSelect(const QPointF &scene_pos);
    void finishLassoSelect();
    void cancelLassoSelect();
    void clearLassoSelect();
    QVector<GraphicsScoreNoteItem *> noteItemsInPath(const QPainterPath &path) const;
    void applyLassoSelection(const QVector<GraphicsScoreNoteItem *> &items);

private:
    // scenes
    QGraphicsScene m_scene_piano;
    PianoRollScene m_scene_roll;

    // cached items
    QList<GraphicsPianoKeyItem *> m_piano_keys;
    QList<QGraphicsRectItem *> m_score_lines;
    QGraphicsPixmapItem *m_score_bg = nullptr;

    std::shared_ptr<Song> m_active_song;
    QMap<int, TrackNoteGroup *> m_track_note_groups;
    NoteDragState m_note_drag;
    NoteCreateState m_note_create;
    NoteDeleteState m_note_delete;
    RectSelectState m_rect_select;
    LassoSelectState m_lasso_select;
    GraphicsSelectionRectItem m_rect_select_preview;
    GraphicsSelectionPathItem m_lasso_select_preview;

    bool m_edits_enabled = true;
    bool m_rect_select_enabled = false;
    bool m_lasso_select_enabled = false;
    bool m_create_notes_enabled = false;
    bool m_delete_notes_enabled = false;
    bool m_ignore_selection_updates = false;
    int m_active_track = -1;
};

#endif // PIANOROLL_H
