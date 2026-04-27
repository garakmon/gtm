#include "app/songeditor.h"

#include "app/editcommands.h"
#include "app/project.h"
#include "sound/song.h"


SongEditor::SongEditor(Project *project, QObject *parent)
  : QObject(parent), m_project(project) {}

bool SongEditor::setActiveSong(const QString &title, QString *error) {
    if (title.isEmpty()) {
        if (error) *error = "Song title is empty.";
        return false;
    }

    std::shared_ptr<Song> song = m_project->getSong(title);
    if (!song) {
        if (error) *error = QString("Song '%1' is not loaded.").arg(title);
        return false;
    }

    QUndoStack *history = song->editHistory();
    if (!history) {
        if (error) *error = QString("Failed to get edit history for '%1'.").arg(title);
        return false;
    }

    if (!m_history_group.stacks().contains(history)) {
        m_history_group.addStack(history);
    }
    m_history_group.setActiveStack(history);
    connect(history, &QUndoStack::cleanChanged,
            this, &SongEditor::onHistoryCleanChanged, Qt::UniqueConnection);
    connect(history, &QUndoStack::indexChanged,
            this, &SongEditor::onHistoryIndexChanged, Qt::UniqueConnection);

    m_song = song.get();
    m_last_history_index = history->index();
    m_selected_events.clear();
    emit selectionChanged();
    return true;
}

void SongEditor::unsetSong() {
    m_history_group.setActiveStack(nullptr);
    m_song = nullptr;
    m_last_history_index = 0;
    m_selected_events.clear();
    emit selectionChanged();
}

bool SongEditor::isSongSet() const {
    return m_song != nullptr;
}

QString SongEditor::activeSongTitle() const {
    if (!m_song) {
        return QString();
    }
    return m_song->getMetaInfo().title;
}

Song *SongEditor::song() const {
    return m_song;
}

QUndoGroup *SongEditor::historyGroup() const {
    return const_cast<QUndoGroup *>(&m_history_group);
}

QUndoStack *SongEditor::editHistory() const {
    return m_history_group.activeStack();
}

bool SongEditor::canUndoHistory() const {
    return m_history_group.canUndo();
}

bool SongEditor::canRedoHistory() const {
    return m_history_group.canRedo();
}

void SongEditor::undo() {
    m_history_group.undo();
}

void SongEditor::redo() {
    m_history_group.redo();
}

void SongEditor::clearHistory() {
    QUndoStack *history = m_history_group.activeStack();
    if (history) {
        history->clear();
    }
}

void SongEditor::setSelectedEvents(const QVector<smf::MidiEvent *> &events) {
    m_selected_events = events;
    emit selectionChanged();
}

void SongEditor::clearSelection() {
    m_selected_events.clear();
    emit selectionChanged();
}

QVector<smf::MidiEvent *> SongEditor::selectedEvents() const {
    return m_selected_events;
}

bool SongEditor::moveSelectedNotes(const NoteMoveSettings &settings, QString *error) {
    if (!validateSong(error)) return false;

    if (m_selected_events.isEmpty()) {
        if (error) *error = "No notes are selected.";
        return false;
    }

    if (settings.delta_tick == 0 && settings.delta_key == 0) {
        return true;
    }

    MoveNotes *cmd = new MoveNotes(m_song, m_selected_events,
                                   settings.delta_tick, settings.delta_key);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::resizeSelectedNotes(const NoteResizeSettings &settings, QString *error) {
    if (!validateSong(error)) return false;

    if (m_selected_events.isEmpty()) {
        if (error) *error = "No notes are selected.";
        return false;
    }

    if (settings.delta_tick == 0) {
        return true;
    }

    ResizeNotes *cmd = new ResizeNotes(m_song, m_selected_events,
                                       settings.delta_tick, settings.resize_end);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::deleteSelectedEvents(QString *error) {
    if (!validateSong(error)) return false;

    if (m_selected_events.isEmpty()) {
        if (error) *error = "No events are selected.";
        return false;
    }

    DeleteNotes *cmd = new DeleteNotes(m_song, m_selected_events);
    editHistory()->push(cmd);
    m_selected_events.clear();
    emit selectionChanged();
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::createNotes(const QVector<NoteCreateSettings> &notes, QString *error) {
    if (!validateSong(error)) return false;

    if (notes.isEmpty()) {
        if (error) *error = "No notes to create.";
        return false;
    }

    CreateNotes *cmd = new CreateNotes(m_song, notes);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::duplicateSelectedNotes(const NoteMoveSettings &settings, QString *error) {
    if (!validateSong(error)) return false;

    if (m_selected_events.isEmpty()) {
        if (error) *error = "No notes are selected.";
        return false;
    }

    DuplicateNotes *cmd = new DuplicateNotes(m_song, m_selected_events,
                                             settings.delta_tick, settings.delta_key);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::addTrack(QString *error) {
    if (!validateSong(error)) return false;

    int non_meta_tracks = 0;
    int track_count = m_song->getTrackCount();
    for (int track = 0; track < track_count; track++) {
        if (!m_song->isMetaTrack(track)) {
            non_meta_tracks++;
        }
    }

    if (non_meta_tracks >= g_max_num_tracks) {
        if (error) {
            *error = QString("Cannot add more than %1 tracks.").arg(g_max_num_tracks);
        }
        return false;
    }

    AddTrack *cmd = new AddTrack(m_song);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::deleteTrack(int track, QString *error) {
    if (!validateSong(error)) return false;

    if (track < 0 || track >= m_song->getTrackCount()) {
        if (error) *error = QString("Invalid track index: %1").arg(track);
        return false;
    }

    m_selected_events.clear();
    emit selectionChanged();

    DeleteTrack *cmd = new DeleteTrack(m_song, track);
    editHistory()->push(cmd);
    emit songEdited(this->activeSongTitle());
    return true;
}

bool SongEditor::addMetaTempo(const TempoEventSettings &, QString *error) {
    if (error) *error = "Add tempo event is not implemented yet.";
    return false;
}

bool SongEditor::addMetaTimeSignature(const TimeSignatureSettings &, QString *error) {
    if (error) *error = "Add time signature event is not implemented yet.";
    return false;
}

bool SongEditor::addMetaKeySignature(const KeySignatureSettings &, QString *error) {
    if (error) *error = "Add key signature event is not implemented yet.";
    return false;
}

bool SongEditor::addControllerEvent(const ControllerEventSettings &, QString *error) {
    if (error) *error = "Add controller event is not implemented yet.";
    return false;
}

bool SongEditor::addProgramChange(const ProgramChangeSettings &, QString *error) {
    if (error) *error = "Add program change event is not implemented yet.";
    return false;
}

bool SongEditor::insertTime(const TimeEditSettings &, QString *error) {
    if (error) *error = "Insert time is not implemented yet.";
    return false;
}

bool SongEditor::deleteTime(const TimeEditSettings &, QString *error) {
    if (error) *error = "Delete time is not implemented yet.";
    return false;
}

bool SongEditor::trimToLastEvent(QString *error) {
    if (error) *error = "Trim to last event is not implemented yet.";
    return false;
}

bool SongEditor::validateSong(QString *error) const {
    if (m_song) {
        return true;
    }
    if (error) *error = "No active song is set in the editor.";
    return false;
}

bool SongEditor::validateTick(int tick, QString *error) const {
    if (tick >= 0) {
        return true;
    }
    if (error) *error = QString("Invalid tick value: %1").arg(tick);
    return false;
}

bool SongEditor::validateChannel(int channel, QString *error) const {
    if (channel >= 0 && channel <= 15) {
        return true;
    }
    if (error) *error = QString("Invalid channel: %1 (must be 0-15)").arg(channel);
    return false;
}

bool SongEditor::validateNoteKey(int key, QString *error) const {
    if (key >= 0 && key <= 127) {
        return true;
    }
    if (error) *error = QString("Invalid note key: %1 (must be 0-127)").arg(key);
    return false;
}

bool SongEditor::validateControllerValue(int value, QString *error) const {
    if (value >= 0 && value <= 127) {
        return true;
    }
    if (error) *error = QString("Invalid controller value: %1 (must be 0-127)").arg(value);
    return false;
}

void SongEditor::markSongDirty() {
    if (m_song) {
        m_song->markDirty();
    }
}

void SongEditor::rebuildCachesAfterEdit() {
    // commands handle their own rebuilds
}

void SongEditor::onHistoryCleanChanged(bool clean) {
    if (!m_song) return;
    m_song->setClean(clean);
    emit songEdited(this->activeSongTitle());
}

void SongEditor::onHistoryIndexChanged(int) {
    QUndoStack *history = editHistory();
    bool rebuild_rolls = false;
    if (!history) {
        emit songNeedsRedrawing(false);
        return;
    }

    const int current_index = history->index();
    const QUndoCommand *base_command = nullptr;
    if (current_index > m_last_history_index && current_index > 0) {
        base_command = history->command(current_index - 1);
    } else if (current_index < m_last_history_index && current_index < history->count()) {
        base_command = history->command(current_index);
    }

    const EditNote *command = dynamic_cast<const EditNote *>(base_command);
    if (command) {
        m_selected_events = command->affectedNoteEvents();
        emit selectionChanged();
    }

    rebuild_rolls = dynamic_cast<const EditTrack *>(base_command) != nullptr;
    m_last_history_index = current_index;
    emit songNeedsRedrawing(rebuild_rolls);
}
