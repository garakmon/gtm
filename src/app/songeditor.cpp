#include "app/songeditor.h"

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

    m_song = song.get();
    m_selected_events.clear();
    emit selectionChanged();
    return true;
}

void SongEditor::unsetSong() {
    m_history_group.setActiveStack(nullptr);
    m_song = nullptr;
    m_selected_events.clear();
    emit selectionChanged();
}

bool SongEditor::isSontSet() const {
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

bool SongEditor::moveSelectedNotes(const NoteMoveSettings &, QString *error) {
    if (error) *error = "Move selected notes is not implemented yet.";
    return false;
}

bool SongEditor::resizeSelectedNotes(const NoteResizeSettings &, QString *error) {
    if (error) *error = "Resize selected notes is not implemented yet.";
    return false;
}

bool SongEditor::deleteSelectedEvents(QString *error) {
    if (error) *error = "Delete selected events is not implemented yet.";
    return false;
}

bool SongEditor::duplicateSelectedNotes(const NoteMoveSettings &, QString *error) {
    if (error) *error = "Duplicate selected notes is not implemented yet.";
    return false;
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

bool SongEditor::validateTick(int, QString *error) const {
    if (error) *error = "Tick validation is not implemented yet.";
    return false;
}

bool SongEditor::validateChannel(int, QString *error) const {
    if (error) *error = "Channel validation is not implemented yet.";
    return false;
}

bool SongEditor::validateNoteKey(int, QString *error) const {
    if (error) *error = "Note key validation is not implemented yet.";
    return false;
}

bool SongEditor::validateControllerValue(int, QString *error) const {
    if (error) *error = "Controller value validation is not implemented yet.";
    return false;
}

void SongEditor::markSongDirty() {
    //
}

void SongEditor::rebuildCachesAfterEdit() {
    //
}
