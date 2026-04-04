#include "ui/pianoroll.h"

#include "sound/song.h"
#include "ui/colors.h"
#include "ui/graphicsscorenoteitem.h"
#include "util/constants.h"
#include "util/logging.h"
#include "util/util.h"

#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>

#include <cmath>
#include <limits>



static bool isScoreNoteItemOrChild(QGraphicsItem *item) {
    QGraphicsItem *current = item;
    while (current) {
        if (dynamic_cast<GraphicsScoreNoteItem *>(current)) {
            return true;
        }
        current = current->parentItem();
    }

    return false;
}

static GraphicsScoreNoteItem *scoreNoteItemAt(QGraphicsScene *scene, const QPointF &scene_pos) {
    if (!scene) {
        return nullptr;
    }

    // drag/sweep delete needs the actual note item under the cursor, but the topmost
    // hit can be a parent/container item in this scene
    // => scan all hits at the point until a score note item is found
    const QList<QGraphicsItem *> items = scene->items(scene_pos);
    for (QGraphicsItem *item : items) {
        QGraphicsItem *current = item;
        while (current) {
            if (auto *note = dynamic_cast<GraphicsScoreNoteItem *>(current)) {
                return note;
            }
            current = current->parentItem();
        }
    }

    return nullptr;
}



PianoRollScene::PianoRollScene(QObject *parent) : QGraphicsScene(parent) {}

void PianoRollScene::setPianoRoll(PianoRoll *piano_roll) {
    m_piano_roll = piano_roll;
}

void PianoRollScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (m_piano_roll && m_piano_roll->isDeleteNotesEnabled()) {
        GraphicsScoreNoteItem *note = scoreNoteItemAt(this, event->scenePos());
        if (note) {
            m_piano_roll->beginNoteDelete(note);
            event->accept();
            return;
        }
    }

    if (m_piano_roll && m_piano_roll->isRectSelectEnabled()) {
        m_piano_roll->beginRectSelect(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->isLassoSelectEnabled()) {
        m_piano_roll->beginLassoSelect(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_create_notes_enabled
     && m_piano_roll->m_active_track < 0) {
        logging::warn("Cannot create note: no active track is selected.",
                      logging::LogCategory::Ui);
    }

    if (m_piano_roll && m_piano_roll->canBeginNoteCreate(event->scenePos())) {
        m_piano_roll->beginNoteCreate(event->scenePos());
        event->accept();
        return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void PianoRollScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_piano_roll && m_piano_roll->m_note_delete.pressed) {
        m_piano_roll->updateNoteDelete(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_note_create.pressed) {
        m_piano_roll->updateNoteCreate(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_rect_select.pressed) {
        m_piano_roll->updateRectSelect(event->scenePos());
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_lasso_select.pressed) {
        m_piano_roll->updateLassoSelect(event->scenePos());
        event->accept();
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void PianoRollScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_piano_roll && m_piano_roll->m_note_delete.pressed) {
        m_piano_roll->updateNoteDelete(event->scenePos());
        m_piano_roll->commitNoteDelete();
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_note_create.pressed) {
        m_piano_roll->updateNoteCreate(event->scenePos());
        m_piano_roll->commitNoteCreate();
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_rect_select.pressed) {
        m_piano_roll->updateRectSelect(event->scenePos());
        m_piano_roll->finishRectSelect();
        event->accept();
        return;
    }

    if (m_piano_roll && m_piano_roll->m_lasso_select.pressed) {
        m_piano_roll->updateLassoSelect(event->scenePos());
        m_piano_roll->finishLassoSelect();
        event->accept();
        return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
}



PianoRoll::PianoRoll(QObject *parent) : QObject(parent) {
    m_scene_piano.setParent(this);
    m_scene_roll.setPianoRoll(this);
    connect(&m_scene_roll, &QGraphicsScene::selectionChanged,
            this, &PianoRoll::updateSelectedEvents);
    m_rect_select_preview.setVisible(false);
    m_lasso_select_preview.setVisible(false);
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
    this->clearNoteCreate();
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
        this->cancelNoteCreate();
        this->cancelNoteDelete();
        this->cancelRectSelect();
        this->cancelLassoSelect();
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

void PianoRoll::setCreateNotesEnabled(bool enabled) {
    m_create_notes_enabled = enabled;
    if (!enabled) {
        this->cancelNoteCreate();
    }
}

void PianoRoll::setDeleteNotesEnabled(bool enabled) {
    m_delete_notes_enabled = enabled;
    if (!enabled) {
        this->cancelNoteDelete();
    }
}

void PianoRoll::setRectSelectEnabled(bool enabled) {
    m_rect_select_enabled = enabled;
    if (!enabled) {
        this->cancelRectSelect();
    }
}

void PianoRoll::setLassoSelectEnabled(bool enabled) {
    m_lasso_select_enabled = enabled;
    if (!enabled) {
        this->cancelLassoSelect();
    }
}

void PianoRoll::setActiveTrack(int track) {
    m_active_track = track;
}

bool PianoRoll::isDeleteNotesEnabled() const {
    return m_delete_notes_enabled;
}

bool PianoRoll::isRectSelectEnabled() const {
    return m_rect_select_enabled;
}

bool PianoRoll::isLassoSelectEnabled() const {
    return m_lasso_select_enabled;
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

/**
 * Select notes whose time ranges intersect the given tick span.
 * This is used by the measure-roll time-range tool, so note pitch is ignored.
 */
void PianoRoll::selectNotesInTickRange(int start_tick, int end_tick, bool modify) {
    const int range_start = std::min(start_tick, end_tick);
    const int range_end = std::max(start_tick, end_tick);
    QList<GraphicsScoreNoteItem *> items;

    for (TrackNoteGroup *group : m_track_note_groups) {
        if (!group) {
            continue;
        }

        for (GraphicsScoreNoteItem *note : group->notes()) {
            if (!note || !note->noteOn() || !note->noteOff()) {
                continue;
            }

            if (!note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                continue;
            }

            const int note_start = note->noteOn()->tick;
            const int note_end = note->noteOff()->tick;
            const bool intersects = note_start < range_end && note_end > range_start;
            if (intersects) {
                items.append(note);
            }
        }
    }

    this->applyNoteSelection(items, modify ? SelectionBehavior::Modify
                                           : SelectionBehavior::Replace);
}

void PianoRoll::invertSelection() {
    m_ignore_selection_updates = true;

    for (TrackNoteGroup *group : m_track_note_groups) {
        if (!group) {
            continue;
        }

        for (GraphicsScoreNoteItem *note : group->notes()) {
            if (!note) {
                continue;
            }

            if (!note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                continue;
            }

            note->setSelected(!note->isSelected());
        }
    }

    m_ignore_selection_updates = false;
    this->updateSelectedEvents();
    m_scene_roll.update();
}

void PianoRoll::selectEvents(const QList<smf::MidiEvent *> &events, bool clear_others) {
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
    if (this->isRectSelectEnabled()) {
        (void)item;
        this->handleRectSelectMousePress(scene_pos);
        return;
    }

    if (this->isLassoSelectEnabled()) {
        (void)item;
        this->handleLassoSelectMousePress(scene_pos);
        return;
    }

    if (this->isDeleteNotesEnabled()) {
        this->beginNoteDelete(item);
        return;
    }

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
void PianoRoll::handleNoteMouseMove(GraphicsScoreNoteItem *item, const QPointF &scene_pos) {
    if (this->isRectSelectEnabled()) {
        (void)item;
        this->handleRectSelectMouseMove(scene_pos);
        return;
    }

    if (this->isLassoSelectEnabled()) {
        (void)item;
        this->handleLassoSelectMouseMove(scene_pos);
        return;
    }

    if (this->isDeleteNotesEnabled()) {
        (void)item;
        this->updateNoteDelete(scene_pos);
        return;
    }

    this->updateNoteDrag(scene_pos);
}

/**
 * Finalize the active note drag interaction.
 */
void PianoRoll::handleNoteMouseRelease(GraphicsScoreNoteItem *item, const QPointF &scene_pos) {
    if (this->isRectSelectEnabled()) {
        (void)item;
        this->handleRectSelectMouseRelease(scene_pos);
        return;
    }

    if (this->isLassoSelectEnabled()) {
        (void)item;
        this->handleLassoSelectMouseRelease(scene_pos);
        return;
    }

    if (this->isDeleteNotesEnabled()) {
        (void)item;
        this->updateNoteDelete(scene_pos);
        this->commitNoteDelete();
        return;
    }

    if (m_note_drag.pressed) {
        this->updateNoteDrag(scene_pos);
        this->commitNoteDrag();
    }
}

void PianoRoll::handleRectSelectMousePress(const QPointF &scene_pos) {
    this->beginRectSelect(scene_pos);
}

void PianoRoll::handleRectSelectMouseMove(const QPointF &scene_pos) {
    this->updateRectSelect(scene_pos);
}

void PianoRoll::handleRectSelectMouseRelease(const QPointF &scene_pos) {
    this->updateRectSelect(scene_pos);
    this->finishRectSelect();
}

void PianoRoll::handleLassoSelectMousePress(const QPointF &scene_pos) {
    this->beginLassoSelect(scene_pos);
}

void PianoRoll::handleLassoSelectMouseMove(const QPointF &scene_pos) {
    this->updateLassoSelect(scene_pos);
}

void PianoRoll::handleLassoSelectMouseRelease(const QPointF &scene_pos) {
    this->updateLassoSelect(scene_pos);
    this->finishLassoSelect();
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

    const QList<GraphicsScoreNoteItem *> notes = this->selectedNoteItems();
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
        // ^ignore click jitter until the drag threshold is crossed
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

    QList<smf::MidiEvent *> events;
    const QList<GraphicsScoreNoteItem *> notes = this->selectedNoteItems();
    events.reserve(notes.size());

    for (GraphicsScoreNoteItem *note : notes) {
        if (!note || !note->noteOn()) {
            continue;
        }
        events.append(note->noteOn());
    }

    emit onSelectedEventsChanged(events);
}

QList<GraphicsScoreNoteItem *> PianoRoll::selectedNoteItems() const {
    QList<GraphicsScoreNoteItem *> notes;
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

/**
 * Snap one absolute tick value to the current note creation grid.
 * This is currently whole-tick snapping (ie, doing nothing), but keeping it here
 * makes it easy to switch note creation to larger grid values later.
 * (eg, I may want to add quarter/8th note snapping, etc)
 */
int PianoRoll::snapTick(int tick) const {
    return std::max(0, tick);
}

int PianoRoll::snapKeyDelta(double delta_y) const {
    const double anchor_y = scoreNotePosition(m_note_drag.anchor_start_key).y + 2;
    const int target_key = this->keyFromSceneY(anchor_y + delta_y);
    return target_key - m_note_drag.anchor_start_key;
}

int PianoRoll::keyFromSceneY(double scene_y) const {
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

/**
 * Check whether a note can be created at this position.
 * This blocks note creation when editing is disabled, no track is active,
 * or the click lands on an existing note.
 */
bool PianoRoll::canBeginNoteCreate(const QPointF &scene_pos) const {
    if (!m_edits_enabled || !m_create_notes_enabled || !m_active_song) {
        return false;
    }

    if (m_active_track < 0) {
        return false;
    }

    QGraphicsItem *hit_item = m_scene_roll.itemAt(scene_pos, QTransform());
    if (isScoreNoteItemOrChild(hit_item)) {
        return false;
    }

    return true;
}

/**
 * Begin creating a note at the clicked position.
 * This stores the starting tick, key, and track, then creates the temporary preview note.
 */
void PianoRoll::beginNoteCreate(const QPointF &scene_pos) {
    if (!this->canBeginNoteCreate(scene_pos)) {
        return;
    }

    const int anchor_tick = this->snapTick(
        static_cast<int>(std::round(scene_pos.x() / ui_tick_x_scale))
    );
    const int anchor_key = this->keyFromSceneY(scene_pos.y());
    const int row = m_active_song->getDisplayRow(m_active_track);

    m_note_create.pressed = true;
    m_note_create.dragging = false;
    m_note_create.track = m_active_track;
    m_note_create.row = row;
    m_note_create.channel = m_active_track;
    m_note_create.velocity = 100;
    m_note_create.press_scene_pos = scene_pos;
    m_note_create.anchor_tick = anchor_tick;
    m_note_create.anchor_key = anchor_key;
    m_note_create.start_tick = anchor_tick;
    m_note_create.duration_ticks = std::max(1, m_active_song->getTicksPerQuarterNote() / 2);
    m_note_create.preview_item = new GraphicsPreviewNoteItem(
        m_note_create.track,
        row,
        m_note_create.start_tick,
        m_note_create.anchor_key,
        m_note_create.duration_ticks
    );
    m_scene_roll.addItem(m_note_create.preview_item);
}

/**
 * Update the preview note while the mouse is dragged.
 * This snaps the note to ticks, supports dragging left or right,
 * and keeps the note at least one tick long.
 */
void PianoRoll::updateNoteCreate(const QPointF &scene_pos) {
    if (!m_note_create.pressed || !m_note_create.preview_item) {
        return;
    }

    const QPointF delta = scene_pos - m_note_create.press_scene_pos;
    if (!m_note_create.dragging && delta.manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    const int current_tick = this->snapTick(
        static_cast<int>(std::round(scene_pos.x() / ui_tick_x_scale))
    );
    m_note_create.dragging = true;

    if (current_tick >= m_note_create.anchor_tick) {
        m_note_create.start_tick = m_note_create.anchor_tick;
        m_note_create.duration_ticks = std::max(1, current_tick - m_note_create.anchor_tick);
    } else {
        m_note_create.start_tick = current_tick;
        m_note_create.duration_ticks = std::max(1, m_note_create.anchor_tick - current_tick);
    }

    this->updatePreviewNoteGeometry();
}

/**
 * Create the previewed note in the song.
 * A click without dragging creates a default eighth-note note.
 */
void PianoRoll::commitNoteCreate() {
    if (!m_note_create.pressed) {
        return;
    }

    if (!m_note_create.preview_item) {
        this->clearNoteCreate();
        return;
    }

    if (!m_note_create.dragging) {
        m_note_create.duration_ticks = std::max(1, m_active_song->getTicksPerQuarterNote() / 2);
        this->updatePreviewNoteGeometry();
    }

    NoteCreateSettings note;
    note.track = m_note_create.track;
    note.channel = m_note_create.channel;
    note.key = m_note_create.anchor_key;
    note.velocity = m_note_create.velocity;
    note.tick = m_note_create.start_tick;
    note.duration = m_note_create.duration_ticks;

    this->clearNoteCreate();
    emit onNoteCreateRequested({note});
}

void PianoRoll::cancelNoteCreate() {
    this->clearNoteCreate();
}

/**
 * Clear the temporary note creation state.
 * This removes the preview note and resets the stored values.
 */
void PianoRoll::clearNoteCreate() {
    m_note_create.pressed = false;
    m_note_create.dragging = false;
    m_note_create.track = -1;
    m_note_create.row = 0;
    m_note_create.channel = 0;
    m_note_create.velocity = 100;
    m_note_create.press_scene_pos = QPointF();
    m_note_create.anchor_tick = 0;
    m_note_create.anchor_key = 60;
    m_note_create.start_tick = 0;
    m_note_create.duration_ticks = 0;
    if (m_note_create.preview_item) {
        m_scene_roll.removeItem(m_note_create.preview_item);
        delete m_note_create.preview_item;
        m_note_create.preview_item = nullptr;
    }
}

/**
 * Update the preview note item from the current creation state.
 * This keeps the preview position and length in sync while dragging.
 */
void PianoRoll::updatePreviewNoteGeometry() {
    if (!m_note_create.preview_item) {
        return;
    }

    m_note_create.preview_item->setTick(m_note_create.start_tick);
    m_note_create.preview_item->setKey(m_note_create.anchor_key);
    m_note_create.preview_item->setTickDuration(m_note_create.duration_ticks);
    m_note_create.preview_item->updatePosition();
}

/**
 * Begin one delete gesture from the clicked note.
 * If the clicked note is already selected, delete the current selection;
 * otherwise clear selection and delete only that note.
 */
void PianoRoll::beginNoteDelete(GraphicsScoreNoteItem *item) {
    if (!m_edits_enabled || !item) {
        return;
    }

    this->clearNoteDelete(true);
    m_note_delete.pressed = true;
    m_note_delete.dragging = false;
    m_note_delete.press_scene_pos = item->scenePos();

    QList<GraphicsScoreNoteItem *> notes;
    if (item->isSelected()) {
        notes = this->selectedNoteItems();
    } else {
        QGraphicsScene *item_scene = item->scene();
        if (item_scene) {
            item_scene->clearSelection();
        }
        item->setSelected(true);
        notes.append(item);
    }

    for (GraphicsScoreNoteItem *note : notes) {
        if (!note || !note->noteOn()
         || m_note_delete.pending_events.contains(note->noteOn())) {
            continue;
        }

        m_note_delete.pending_events.insert(note->noteOn());
        m_note_delete.hidden_items.append(note);
        note->hide();
    }
}

/**
 * Extend one delete sweep across notes under the cursor.
 * The drag threshold prevents a plain click-release on stacked notes
 * from also deleting the next note underneath on release.
 */
void PianoRoll::updateNoteDelete(const QPointF &scene_pos) {
    if (!m_note_delete.pressed) {
        return;
    }

    const QPointF delta = scene_pos - m_note_delete.press_scene_pos;
    if (!m_note_delete.dragging && delta.manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    m_note_delete.dragging = true;

    GraphicsScoreNoteItem *item = scoreNoteItemAt(&m_scene_roll, scene_pos);
    if (!item || !item->noteOn()) {
        return;
    }

    if (m_note_delete.pending_events.contains(item->noteOn())) {
        return;
    }

    m_note_delete.pending_events.insert(item->noteOn());
    m_note_delete.hidden_items.append(item);
    item->hide();
}

/**
 * Commit the current delete sweep as one delete request.
 * Notes are hidden immediately during the drag, but the song data
 * is only modified here so undo history stays grouped.
 */
void PianoRoll::commitNoteDelete() {
    if (!m_note_delete.pressed) {
        return;
    }

    QList<smf::MidiEvent *> events;
    events.reserve(m_note_delete.pending_events.size());
    for (smf::MidiEvent *event : m_note_delete.pending_events) {
        events.append(event);
    }

    this->clearNoteDelete(false);
    if (!events.isEmpty()) {
        emit onNoteDeleteRequested(events);
    }
}

void PianoRoll::cancelNoteDelete() {
    this->clearNoteDelete(true);
}

void PianoRoll::clearNoteDelete(bool restore_visibility) {
    if (restore_visibility) {
        for (GraphicsScoreNoteItem *item : m_note_delete.hidden_items) {
            if (item) {
                item->show();
            }
        }
    }

    m_note_delete.pressed = false;
    m_note_delete.dragging = false;
    m_note_delete.press_scene_pos = QPointF();
    m_note_delete.pending_events.clear();
    m_note_delete.hidden_items.clear();
}

/**
 * Begin a rectangular selection drag at the current scene position.
 * The preview box stays hidden until the drag threshold is crossed.
 */
void PianoRoll::beginRectSelect(const QPointF &scene_pos) {
    this->clearRectSelect();

    m_rect_select.pressed = true;
    m_rect_select.dragging = false;
    m_rect_select.start_scene_pos = scene_pos;
    m_rect_select.current_scene_pos = scene_pos;
    m_rect_select.behavior = this->selectionBehaviorForModifiers(QApplication::keyboardModifiers());
    m_rect_select_preview.setSelectionRect(QRectF(scene_pos, scene_pos));
    m_rect_select_preview.setVisible(false);
    m_scene_roll.addItem(&m_rect_select_preview);
}

/**
 * Update the rectangular selection preview while the mouse is dragged.
 * A simple click should not create a visible selection box.
 */
void PianoRoll::updateRectSelect(const QPointF &scene_pos) {
    if (!m_rect_select.pressed) {
        return;
    }

    m_rect_select.current_scene_pos = scene_pos;
    if (!m_rect_select.dragging) {
        const QPointF delta = scene_pos - m_rect_select.start_scene_pos;
        if (std::hypot(delta.x(), delta.y()) < QApplication::startDragDistance()) {
            return;
        }

        m_rect_select.dragging = true;
        m_rect_select_preview.setVisible(true);
    }

    m_rect_select_preview.setSelectionRect(this->currentRectSelectRect());
}

/**
 * Finish the rectangular selection gesture.
 * A click selects the clicked note or clears selection, while a drag
 * replaces the selection with notes intersecting the rectangle.
 * A click on empty space without dragging just clears the current selection.
 */
void PianoRoll::finishRectSelect() {
    if (!m_rect_select.pressed) {
        return;
    }

    if (!m_rect_select.dragging) {
        GraphicsScoreNoteItem *note = scoreNoteItemAt(&m_scene_roll,
                                                      m_rect_select.start_scene_pos);
        if (m_rect_select.behavior == SelectionBehavior::Replace) {
            m_scene_roll.clearSelection();
            if (note && note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                note->setSelected(true);
                this->updateSelectedEvents();
            }
        }
        else {
            if (!note) {
                m_scene_roll.clearSelection();
                this->updateSelectedEvents();
            }
            else if (note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                note->setSelected(!note->isSelected());
                this->updateSelectedEvents();
            }
        }
        this->clearRectSelect();
        return;
    }

    const QRectF rect = this->currentRectSelectRect();
    const QList<GraphicsScoreNoteItem *> items = this->noteItemsInRect(rect);
    this->applyNoteSelection(items, m_rect_select.behavior);
    this->clearRectSelect();
}

void PianoRoll::cancelRectSelect() {
    this->clearRectSelect();
}

void PianoRoll::clearRectSelect() {
    if (m_rect_select_preview.scene() == &m_scene_roll) {
        m_scene_roll.removeItem(&m_rect_select_preview);
    }
    m_rect_select_preview.setVisible(false);

    m_rect_select.pressed = false;
    m_rect_select.dragging = false;
    m_rect_select.start_scene_pos = QPointF();
    m_rect_select.current_scene_pos = QPointF();
}

QRectF PianoRoll::currentRectSelectRect() const {
    return QRectF(m_rect_select.start_scene_pos,
                  m_rect_select.current_scene_pos).normalized();
}

QList<GraphicsScoreNoteItem *> PianoRoll::noteItemsInRect(const QRectF &rect) const {
    QList<GraphicsScoreNoteItem *> notes;
    const QList<QGraphicsItem *> items = m_scene_roll.items(
        rect, Qt::IntersectsItemShape, Qt::DescendingOrder, QTransform()
    );

    for (QGraphicsItem *item : items) {
        auto *note = dynamic_cast<GraphicsScoreNoteItem *>(item);
        if (!note) {
            continue;
        }

        if (!note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
            continue;
        }

        notes.append(note);
    }

    return notes;
}

/**
 * Apply a note selection using the current selection behavior.
 * Modify mode removes the touched set only when it is already fully selected;
 * otherwise it adds that touched set to the current selection.
 */
void PianoRoll::applyNoteSelection(const QList<GraphicsScoreNoteItem *> &items,
                                   SelectionBehavior behavior) {
    m_ignore_selection_updates = true;

    if (behavior == SelectionBehavior::Replace) {
        m_scene_roll.clearSelection();
        for (GraphicsScoreNoteItem *note : items) {
            if (note) {
                note->setSelected(true);
            }
        }
    }
    else {
        bool all_selected = !items.isEmpty();
        for (GraphicsScoreNoteItem *note : items) {
            if (!note || !note->isSelected()) {
                all_selected = false;
                break;
            }
        }

        const bool deselect_all = all_selected;
        for (GraphicsScoreNoteItem *note : items) {
            if (!note) {
                continue;
            }

            note->setSelected(!deselect_all);
        }
    }

    m_ignore_selection_updates = false;
    this->updateSelectedEvents();
}

/**
 * Begin a lasso selection drag at the current scene position.
 * The preview path stays hidden until the drag threshold is crossed.
 */
void PianoRoll::beginLassoSelect(const QPointF &scene_pos) {
    this->clearLassoSelect();

    m_lasso_select.pressed = true;
    m_lasso_select.dragging = false;
    m_lasso_select.start_scene_pos = scene_pos;
    m_lasso_select.behavior = this->selectionBehaviorForModifiers(QApplication::keyboardModifiers());
    m_lasso_select.path = QPainterPath(scene_pos);
    m_lasso_select_preview.setSelectionPath(m_lasso_select.path);
    m_lasso_select_preview.setVisible(false);
    m_scene_roll.addItem(&m_lasso_select_preview);
}

/**
 * Update the lasso path while the mouse is dragged.
 * A simple click should not create a visible lasso preview.
 */
void PianoRoll::updateLassoSelect(const QPointF &scene_pos) {
    if (!m_lasso_select.pressed) {
        return;
    }

    if (!m_lasso_select.dragging) {
        const QPointF delta = scene_pos - m_lasso_select.start_scene_pos;
        if (std::hypot(delta.x(), delta.y()) < QApplication::startDragDistance()) {
            return;
        }

        m_lasso_select.dragging = true;
        m_lasso_select_preview.setVisible(true);
    }

    m_lasso_select.path.lineTo(scene_pos);
    m_lasso_select_preview.setSelectionPath(m_lasso_select.path);
}

/**
 * Finish the lasso selection gesture.
 * A click selects the clicked note or clears selection, while a drag
 * replaces the selection with notes intersecting the lasso path.
 */
void PianoRoll::finishLassoSelect() {
    if (!m_lasso_select.pressed) {
        return;
    }

    if (!m_lasso_select.dragging) {
        GraphicsScoreNoteItem *note = scoreNoteItemAt(&m_scene_roll,
                                                      m_lasso_select.start_scene_pos);
        if (m_lasso_select.behavior == SelectionBehavior::Replace) {
            m_scene_roll.clearSelection();
            if (note && note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                note->setSelected(true);
                this->updateSelectedEvents();
            }
        }
        else {
            if (!note) {
                m_scene_roll.clearSelection();
                this->updateSelectedEvents();
            }
            else if (note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
                note->setSelected(!note->isSelected());
                this->updateSelectedEvents();
            }
        }
        this->clearLassoSelect();
        return;
    }

    QPainterPath closed_path = m_lasso_select.path;
    closed_path.closeSubpath();
    const QList<GraphicsScoreNoteItem *> items = this->noteItemsInPath(closed_path);
    this->applyNoteSelection(items, m_lasso_select.behavior);
    this->clearLassoSelect();
}

void PianoRoll::cancelLassoSelect() {
    this->clearLassoSelect();
}

void PianoRoll::clearLassoSelect() {
    if (m_lasso_select_preview.scene() == &m_scene_roll) {
        m_scene_roll.removeItem(&m_lasso_select_preview);
    }
    m_lasso_select_preview.setVisible(false);

    m_lasso_select.pressed = false;
    m_lasso_select.dragging = false;
    m_lasso_select.start_scene_pos = QPointF();
    m_lasso_select.path = QPainterPath();
}

QList<GraphicsScoreNoteItem *> PianoRoll::noteItemsInPath(const QPainterPath &path) const {
    QList<GraphicsScoreNoteItem *> notes;
    const QList<QGraphicsItem *> items = m_scene_roll.items(
        path, Qt::IntersectsItemShape, Qt::DescendingOrder, QTransform()
    );

    for (QGraphicsItem *item : items) {
        auto *note = dynamic_cast<GraphicsScoreNoteItem *>(item);
        if (!note) {
            continue;
        }

        if (!note->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
            continue;
        }

        notes.append(note);
    }

    return notes;
}

/**
 * Determine the selection behavior from keyboard modifiers at gesture start.
 * Holding Ctrl/Cmd enters modify mode, while no modifier = replace the current selection.
 */
PianoRoll::SelectionBehavior PianoRoll::selectionBehaviorForModifiers(
    Qt::KeyboardModifiers modifiers
) const {
    if (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier)) {
        return SelectionBehavior::Modify;
    }

    return SelectionBehavior::Replace;
}
