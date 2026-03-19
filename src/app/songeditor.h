#pragma once
#ifndef SONGEDITOR_H
#define SONGEDITOR_H

#include "app/structs.h"
#include "deps/midifile/MidiEvent.h"

#include <QObject>
#include <QString>
#include <QUndoStack>
#include <QUndoGroup>
#include <QVector>



class Project;
class Song;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// SongEditor edits one active song at a time and coordinates all the editing functions.
///
//////////////////////////////////////////////////////////////////////////////////////////
class SongEditor : public QObject {
    Q_OBJECT

public:
    explicit SongEditor(Project *project, QObject *parent = nullptr);
    ~SongEditor() override = default;

    // disable copy and move
    SongEditor(const SongEditor &) = delete;
    SongEditor &operator=(const SongEditor &) = delete;
    SongEditor(SongEditor &&) = delete;
    SongEditor &operator=(SongEditor &&) = delete;

    // song binding
    bool setActiveSong(const QString &title, QString *error = nullptr);
    void unsetSong();
    bool isSongSet() const;
    QString activeSongTitle() const;
    Song *song() const;
    QUndoGroup *historyGroup() const;

    // edit history
    QUndoStack *editHistory() const;
    bool canUndoHistory() const;
    bool canRedoHistory() const;
    void undo();
    void redo();
    void clearHistory();

    // selection
    void setSelectedEvents(const QVector<smf::MidiEvent *> &events);
    void clearSelection();
    QVector<smf::MidiEvent *> selectedEvents() const;

    // note edits
    bool moveSelectedNotes(const NoteMoveSettings &settings, QString *error = nullptr);
    bool resizeSelectedNotes(const NoteResizeSettings &settings, QString *error = nullptr);
    bool deleteSelectedEvents(QString *error = nullptr);
    bool createNotes(const QVector<NoteCreateSettings> &notes, QString *error = nullptr);
    bool duplicateSelectedNotes(const NoteMoveSettings &settings, QString *error = nullptr);

    // event edits
    bool addMetaTempo(const TempoEventSettings &settings, QString *error = nullptr);
    bool addMetaTimeSignature(const TimeSignatureSettings &settings, QString *error = nullptr);
    bool addMetaKeySignature(const KeySignatureSettings &settings, QString *error = nullptr);
    bool addControllerEvent(const ControllerEventSettings &settings, QString *error = nullptr);
    bool addProgramChange(const ProgramChangeSettings &settings, QString *error = nullptr);

    // time edits
    bool insertTime(const TimeEditSettings &settings, QString *error = nullptr);
    bool deleteTime(const TimeEditSettings &settings, QString *error = nullptr);
    bool trimToLastEvent(QString *error = nullptr);
    // duration ops? or does having a "duration" not make sense since it's last event tick
    // !TODO ^

signals:
    void songEdited(const QString &title);
    void selectionChanged();

private:
    // validation
    bool validateSong(QString *error) const;
    bool validateTick(int tick, QString *error) const;
    bool validateChannel(int channel, QString *error) const;
    bool validateNoteKey(int key, QString *error) const;
    bool validateControllerValue(int value, QString *error) const;

    void markSongDirty();
    void rebuildCachesAfterEdit();
    void onHistoryCleanChanged(bool clean);

private:
    Project *m_project = nullptr;
    QUndoGroup m_history_group;

    Song *m_song = nullptr;

    QVector<smf::MidiEvent *> m_selected_events;
};

#endif // SONGEDITOR_H
