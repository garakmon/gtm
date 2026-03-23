#include "ui/pianoroll.h"

#include "sound/song.h"
#include "ui/colors.h"
#include "ui/graphicsscorenoteitem.h"
#include "util/constants.h"
#include "util/util.h"

#include <QPainter>
#include <QPixmap>
#include <QApplication>

#include <cmath>
#include <limits>



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    m_scene_piano.setParent(this);
    connect(&m_scene_roll, &QGraphicsScene::selectionChanged,
            this, &PianoRoll::updateSelectedEvents);
    this->drawPiano();
}

QGraphicsScene *PianoRoll::scenePiano() {
    return &m_scene_piano;
}

QGraphicsScene *PianoRoll::sceneRoll() {
    return &m_scene_roll;
}

QList<GraphicsPianoKeyItem *> PianoRoll::keys() {
    return m_piano_keys;
}

QList<QGraphicsRectItem *> PianoRoll::lines() {
    return m_score_lines;
}

/**
 * Set the active song and clear the current roll items.
 */
void PianoRoll::setSong(std::shared_ptr<Song> song) {
    this->clearNoteDrag();
    m_ignore_selection_updates = true;
    m_score_bg = nullptr;
    m_scene_roll.clear();
    m_track_note_groups.clear();
    m_active_song = song;
    m_ignore_selection_updates = false;
}

/**
 * Add one note item to the roll.
 * The event is guaranteed to be a NoteOn event.
 */
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

    GraphicsScoreNoteItem *item = new GraphicsScoreNoteItem(this, track, row, event,
                                                            event->getLinkedEvent());
    item->setParentItem(group);
    item->setFlag(QGraphicsItem::ItemIsSelectable, m_edits_enabled);
    item->setAcceptedMouseButtons(m_edits_enabled ? Qt::LeftButton : Qt::NoButton);
    group->addNote(item);

    return item;
}

/**
 * Draw the score area and note items.
 */
void PianoRoll::display() {
    this->drawScoreArea();
    this->drawScoreNotes();
}

/**
 * Enable or disable note editing.
 * Note: when the song is playing, editing would be too complicated.
 */
void PianoRoll::setEditsEnabled(bool enabled) {
    m_edits_enabled = enabled;
    if (!enabled) {
        this->cancelNoteDrag();
    }

    for (TrackNoteGroup *group : m_track_note_groups) {
        if (!group) continue;

        for (auto *note : group->notes()) {
            if (!note) continue;
            note->setFlag(QGraphicsItem::ItemIsSelectable, enabled);
            note->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
        }
    }

    m_scene_roll.clearSelection();
    m_scene_roll.invalidate();
    m_scene_roll.update();
}

/**
 * Select all notes for one track.
 */
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

void PianoRoll::selectEvents(const QVector<smf::MidiEvent *> &events, bool clear_others) {
    QSet<smf::MidiEvent *> selected_events(events.begin(), events.end());

    m_ignore_selection_updates = true;

    if (clear_others) {
        m_scene_roll.clearSelection();
    }

    for (TrackNoteGroup *group : m_track_note_groups) {
        if (!group) {
            continue;
        }

        for (GraphicsScoreNoteItem *note : group->notes()) {
            if (!note || !note->noteOn()) {
                continue;
            }

            if (!note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                continue;
            }

            note->setSelected(selected_events.contains(note->noteOn()));
        }
    }

    m_ignore_selection_updates = false;
    this->updateSelectedEvents();
    m_scene_roll.update();
}

/**
 * Resolve the drag mode for a note press and begin the interaction.
 */
void PianoRoll::handleNoteMousePress(GraphicsScoreNoteItem *item,
                                     const QPointF &scene_pos,
                                     bool resize_start, bool resize_end) {
    if (resize_start) {
        m_note_drag.mode = NoteDragMode::ResizeStart;
    } else if (resize_end) {
        m_note_drag.mode = NoteDragMode::ResizeEnd;
    } else {
        m_note_drag.mode = NoteDragMode::Move;
    }
    this->beginNoteDrag(item, scene_pos);
}

/**
 * Update the active note drag preview from the latest mouse position.
 */
void PianoRoll::handleNoteMouseMove(GraphicsScoreNoteItem *, const QPointF &scene_pos) {
    this->updateNoteDrag(scene_pos);
}

/**
 * Finalize the active note drag interaction.
 */
void PianoRoll::handleNoteMouseRelease(GraphicsScoreNoteItem *, const QPointF &scene_pos) {
    if (m_note_drag.pressed) {
        this->updateNoteDrag(scene_pos);
        this->commitNoteDrag();
    }
}

/**
 * Draw the piano keyboard scene.
 */
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
        m_piano_keys.append(item);
        m_scene_piano.addItem(item);
    }

    QRectF rect = m_scene_piano.itemsBoundingRect();
    m_scene_piano.setSceneRect(rect);
}

/**
 * Draw the cached score background.
 */
void PianoRoll::drawScoreArea() {
    m_score_lines.clear();
    if (m_score_bg) {
        if (m_score_bg->scene() == &m_scene_roll) {
            m_scene_roll.removeItem(m_score_bg);
        }

        delete m_score_bg;
        m_score_bg = nullptr;
    }

    int final_tick = m_active_song->durationInTicks();
    int tpqn = m_active_song->getTicksPerQuarterNote();
    int grid_w = qMax(1, final_tick * ui_tick_x_scale);
    int grid_h = ui_score_line_height * g_num_notes_piano;

    QPixmap bg_pix(grid_w, grid_h);
    bg_pix.fill(ui_color_piano_roll_bg);

    {
        QPainter p(&bg_pix);
        p.setRenderHint(QPainter::Antialiasing, false);

        for (int i = 0; i < g_num_notes_piano; i++) {
            int y = scoreNotePosition(i).y;
            int h = scoreNotePosition(i).height;
            QColor fill = isNoteWhite(i) ? ui_color_score_line_light
                                         : ui_color_score_line_dark;
            QColor line = isNoteWhite(i) ? ui_color_score_line_dark
                                         : ui_color_score_line_light;
            p.fillRect(QRect(0, y, grid_w, h), fill);
            p.setPen(line);
            p.drawLine(0, y + h - 1, grid_w, y + h - 1);
        }

        // draw vertical beat lines aligned to measure boundaries
        auto &time_sigs = m_active_song->getTimeSignatures();
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

            int ticks_per_beat = tpqn * 4 / current_den;
            int ticks_per_measure = current_num * ticks_per_beat;

            auto next_it = std::next(it_sig);
            int end_of_segment = next_it != time_sigs.end() ? next_it.key()
                                                            : final_tick;

            while (current_tick < end_of_segment) {
                int measure_start_tick = current_tick;
                int measure_end_tick = std::min(current_tick + ticks_per_measure,
                                                end_of_segment);

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
                ++it_sig;
            }
        }
    }

    m_score_bg = m_scene_roll.addPixmap(bg_pix);
    m_score_bg->setZValue(-2);
    m_score_bg->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    m_scene_roll.setBackgroundBrush(Qt::NoBrush);
    m_scene_roll.setSceneRect(0, 0, grid_w, grid_h);
}

/**
 * Draw all note items for the active song.
 */
void PianoRoll::drawScoreNotes() {
    for (auto [track, note_event] : m_active_song->getNotes()) {
        int row = m_active_song->getDisplayRow(track);
        this->addNote(track, row, note_event);
    }
}

/**
 * Get the selected notes and their starting geometry for one drag movement.
 */
void PianoRoll::beginNoteDrag(GraphicsScoreNoteItem *item, const QPointF &scene_pos) {
    if (!m_edits_enabled || !item) {
        return;
    }

    m_note_drag.pressed = true;
    m_note_drag.dragging = false;
    m_note_drag.anchor_item = item;
    m_note_drag.drag_start_scene_pos = scene_pos;
    m_note_drag.anchor_start_tick = item->noteOn()->tick;
    m_note_drag.anchor_start_key = item->noteOn()->getKeyNumber();
    m_note_drag.delta_tick = 0;
    m_note_drag.delta_key = 0;
    m_note_drag.notes.clear();

    const QVector<GraphicsScoreNoteItem *> notes = this->selectedNoteItems();
    m_note_drag.notes.reserve(notes.size());
    for (GraphicsScoreNoteItem *note : notes) {
        if (!note) {
            continue;
        }

        DraggedNoteState state;
        state.item = note;
        state.note_on = note->noteOn();
        state.start_tick = note->noteOn()->tick;
        state.start_key = note->noteOn()->getKeyNumber();
        state.start_duration = note->tickDuration();
        state.start_pos = note->pos();
        m_note_drag.notes.append(state);
    }
}

/**
 * Update the drag preview for either moving or resizing notes.
 */
void PianoRoll::updateNoteDrag(const QPointF &scene_pos) {
    if (!m_note_drag.pressed || !m_note_drag.anchor_item || m_note_drag.notes.isEmpty()) {
        return;
    }

    const QPointF delta = scene_pos - m_note_drag.drag_start_scene_pos;
    if (!m_note_drag.dragging && delta.manhattanLength() < QApplication::startDragDistance()) {
        // ignore click jitter until the drag threshold is crossed
        return;
    }

    m_note_drag.dragging = true;

    int delta_tick = this->snapTickDelta(delta.x());
    if (m_note_drag.mode == NoteDragMode::Move) {
        int delta_key = this->snapKeyDelta(delta.y());
        this->clampDraggedDelta(&delta_tick, &delta_key);

        m_note_drag.delta_tick = delta_tick;
        m_note_drag.delta_key = delta_key;

        for (const DraggedNoteState &state : m_note_drag.notes) {
            if (!state.item) {
                continue;
            }

            const int tick = state.start_tick + delta_tick;
            const int key = state.start_key + delta_key;
            const int x = tick * ui_tick_x_scale;
            const int y = scoreNotePosition(key).y + 2;
            state.item->setPos(QPointF(x, y));
        }
        return;
    }

    m_note_drag.delta_tick = this->clampResizeDelta(delta_tick);
    m_note_drag.delta_key = 0;
    this->updateResizePreview();
}

/**
 * Commit the current drag gesture as one edit command.
 */
void PianoRoll::commitNoteDrag() {
    if (!m_note_drag.pressed) {
        return;
    }

    const bool dragging = m_note_drag.dragging;
    const int delta_tick = m_note_drag.delta_tick;
    const int delta_key = m_note_drag.delta_key;

    if (!dragging || (delta_tick == 0 && delta_key == 0)) {
        this->restoreDraggedNotesToStart();
        this->clearNoteDrag();
        return;
    }

    if (m_note_drag.mode == NoteDragMode::Move) {
        NoteMoveSettings settings;
        settings.delta_tick = delta_tick;
        settings.delta_key = delta_key;
        emit onNoteMoveRequested(settings);
    } else {
        NoteResizeSettings settings;
        settings.delta_tick = delta_tick;
        settings.resize_end = m_note_drag.mode == NoteDragMode::ResizeEnd;
        emit onNoteResizeRequested(settings);
    }

    this->clearNoteDrag();
}

/**
 * Cancel the active drag and restore the previewed notes.
 */
void PianoRoll::cancelNoteDrag() {
    this->restoreDraggedNotesToStart();
    this->clearNoteDrag();
}

/**
 * Reset the stored drag session state.
 */
void PianoRoll::clearNoteDrag() {
    m_note_drag.pressed = false;
    m_note_drag.dragging = false;
    m_note_drag.anchor_item = nullptr;
    m_note_drag.mode = NoteDragMode::Move;
    m_note_drag.drag_start_scene_pos = QPointF();
    m_note_drag.anchor_start_tick = 0;
    m_note_drag.anchor_start_key = 0;
    m_note_drag.delta_tick = 0;
    m_note_drag.delta_key = 0;
    m_note_drag.notes.clear();
}

/**
 * Restore note positions and temporary widths after a canceled drag.
 */
void PianoRoll::restoreDraggedNotesToStart() {
    for (const DraggedNoteState &state : m_note_drag.notes) {
        if (!state.item) {
            continue;
        }
        state.item->setPos(state.start_pos);
        state.item->clearPreviewDurationTicks();
    }
}

/**
 * Sync the editor selection with the currently selected note items.
 */
void PianoRoll::updateSelectedEvents() {
    if (m_ignore_selection_updates) {
        return;
    }

    QVector<smf::MidiEvent *> events;
    const QVector<GraphicsScoreNoteItem *> notes = this->selectedNoteItems();
    events.reserve(notes.size());

    for (GraphicsScoreNoteItem *note : notes) {
        if (!note || !note->noteOn()) {
            continue;
        }
        events.append(note->noteOn());
    }

    emit onSelectedEventsChanged(events);
}

QVector<GraphicsScoreNoteItem *> PianoRoll::selectedNoteItems() const {
    QVector<GraphicsScoreNoteItem *> notes;
    const QList<QGraphicsItem *> items = m_scene_roll.selectedItems();
    notes.reserve(items.size());

    for (QGraphicsItem *item : items) {
        GraphicsScoreNoteItem *note = dynamic_cast<GraphicsScoreNoteItem *>(item);
        if (!note) {
            continue;
        }
        notes.append(note);
    }

    return notes;
}

int PianoRoll::snapTickDelta(double delta_x) const {
    return qRound(delta_x / ui_tick_x_scale);
}

int PianoRoll::snapKeyDelta(double delta_y) const {
    const double anchor_y = scoreNotePosition(m_note_drag.anchor_start_key).y + 2;
    const int target_key = this->keyForSceneY(anchor_y + delta_y);
    return target_key - m_note_drag.anchor_start_key;
}

int PianoRoll::keyForSceneY(double scene_y) const {
    int best_key = 0;
    double best_distance = std::numeric_limits<double>::max();

    for (int key = 0; key < g_num_notes_piano; key++) {
        const double key_y = scoreNotePosition(key).y + 2;
        const double distance = std::abs(scene_y - key_y);
        if (distance < best_distance) {
            best_distance = distance;
            best_key = key;
        }
    }

    return best_key;
}

void PianoRoll::clampDraggedDelta(int *delta_tick, int *delta_key) const {
    if (!delta_tick || !delta_key) {
        return;
    }

    int min_tick_delta = *delta_tick;
    int min_key_delta = *delta_key;
    int max_key_delta = *delta_key;

    for (const DraggedNoteState &state : m_note_drag.notes) {
        min_tick_delta = std::max(min_tick_delta, -state.start_tick);
        min_key_delta = std::max(min_key_delta, -state.start_key);
        max_key_delta = std::min(max_key_delta, 127 - state.start_key);
    }

    *delta_tick = min_tick_delta;
    *delta_key = std::clamp(*delta_key, min_key_delta, max_key_delta);
}

/**
 * Clamp a resize delta so every affected note remains valid.
 */
int PianoRoll::clampResizeDelta(int delta_tick) const {
    int min_delta = delta_tick;
    int max_delta = delta_tick;

    for (const DraggedNoteState &state : m_note_drag.notes) {
        if (m_note_drag.mode == NoteDragMode::ResizeEnd) {
            // note length must stay at least one tick
            min_delta = std::max(min_delta, 1 - state.start_duration);
            continue;
        }

        min_delta = std::max(min_delta, -state.start_tick);
        max_delta = std::min(max_delta, state.start_duration - 1);
    }

    return std::clamp(delta_tick, min_delta, max_delta);
}

/**
 * Preview note widths and note starts during a resize drag.
 */
void PianoRoll::updateResizePreview() {
    for (const DraggedNoteState &state : m_note_drag.notes) {
        if (!state.item) {
            continue;
        }

        int start_tick = state.start_tick;
        int duration = state.start_duration;
        if (m_note_drag.mode == NoteDragMode::ResizeEnd) {
            duration += m_note_drag.delta_tick;
        } else {
            // resizing the start shifts the note and shortens from the left
            start_tick += m_note_drag.delta_tick;
            duration -= m_note_drag.delta_tick;
        }

        const int x = start_tick * ui_tick_x_scale;
        state.item->setPos(QPointF(x, state.start_pos.y()));
        state.item->setPreviewDurationTicks(duration);
    }
}
