#pragma once
#ifndef EDITCOMMANDS_H
#define EDITCOMMANDS_H

#include "app/structs.h"
#include "deps/midifile/MidiEvent.h"

#include <QUndoCommand>
#include <QVector>

#include <vector>



enum CommandId {
    ID_MoveNotes = 0,
    ID_ResizeNotes,
    ID_DeleteNotes,
    ID_CreateNotes,
    ID_DuplicateNotes,
};

class Song;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Base undo command for all note edit operations.
///
//////////////////////////////////////////////////////////////////////////////////////////
class EditNote : public QUndoCommand {
public:
    explicit EditNote(Song *song, const QString &text,
                      QUndoCommand *parent = nullptr);
    ~EditNote() override = default;

    EditNote(const EditNote &) = delete;
    EditNote &operator=(const EditNote &) = delete;
    EditNote(EditNote &&) = delete;
    EditNote &operator=(EditNote &&) = delete;

    virtual QVector<smf::MidiEvent *> affectedNoteEvents() const = 0;

protected:
    Song *m_song = nullptr;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Move selected notes by a tick and/or key delta.
/// Supports mergeWith() for smooth drag → single undo step.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MoveNotes : public EditNote {
public:
    MoveNotes(Song *song, const QVector<smf::MidiEvent *> &notes,
              int delta_tick, int delta_key);

    void redo() override;
    void undo() override;
    int id() const override { return ID_MoveNotes; }
    bool mergeWith(const QUndoCommand *other) override;
    QVector<smf::MidiEvent *> affectedNoteEvents() const override;

private:
    struct NoteState {
        smf::MidiEvent *note_on;
        smf::MidiEvent *note_off;
        int orig_tick_on;
        int orig_tick_off;
        int orig_key;
    };
    QVector<NoteState> m_states;
    int m_delta_tick;
    int m_delta_key;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Resize selected notes by a tick delta on either end.
/// Supports mergeWith() for smooth drag-resize → single undo step.
///
//////////////////////////////////////////////////////////////////////////////////////////
class ResizeNotes : public EditNote {
public:
    ResizeNotes(Song *song, const QVector<smf::MidiEvent *> &notes,
                int delta_tick, bool resize_end);

    void redo() override;
    void undo() override;
    int id() const override { return ID_ResizeNotes; }
    bool mergeWith(const QUndoCommand *other) override;
    QVector<smf::MidiEvent *> affectedNoteEvents() const override;

private:
    struct NoteState {
        smf::MidiEvent *note_on;
        smf::MidiEvent *target; // note-off if resize_end, note-on otherwise
        smf::MidiEvent *other;  // the paired event, for min duration clamping
        int orig_tick;
    };
    QVector<NoteState> m_states;
    int m_delta_tick;
    bool m_resize_end;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Delete selected notes by marking their events as empty.
///
//////////////////////////////////////////////////////////////////////////////////////////
class DeleteNotes : public EditNote {
public:
    DeleteNotes(Song *song, const QVector<smf::MidiEvent *> &notes);

    void redo() override;
    void undo() override;
    int id() const override { return ID_DeleteNotes; }
    QVector<smf::MidiEvent *> affectedNoteEvents() const override;

private:
    struct DeletedNote {
        smf::MidiEvent *note_on;
        smf::MidiEvent *note_off;
        std::vector<smf::uchar> on_bytes;
        std::vector<smf::uchar> off_bytes;
        int on_tick;
        int off_tick;
        int track;
    };
    QVector<DeletedNote> m_notes;
    bool m_first_redo = true;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Create new notes from a list of NoteCreateSettings.
///
//////////////////////////////////////////////////////////////////////////////////////////
class CreateNotes : public EditNote {
public:
    CreateNotes(Song *song, const QVector<NoteCreateSettings> &notes);

    void redo() override;
    void undo() override;
    int id() const override { return ID_CreateNotes; }
    QVector<smf::MidiEvent *> affectedNoteEvents() const override;

private:
    QVector<NoteCreateSettings> m_settings;
    QVector<smf::MidiEvent *> m_created_on;
    QVector<smf::MidiEvent *> m_created_off;
    bool m_first_redo = true;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Duplicate selected notes with a tick and key offset.
///
//////////////////////////////////////////////////////////////////////////////////////////
class DuplicateNotes : public EditNote {
public:
    DuplicateNotes(Song *song, const QVector<smf::MidiEvent *> &source_notes,
                   int delta_tick, int delta_key);

    void redo() override;
    void undo() override;
    int id() const override { return ID_DuplicateNotes; }
    QVector<smf::MidiEvent *> affectedNoteEvents() const override;

private:
    struct SourceNote {
        int track;
        int channel;
        int key;
        int velocity;
        int tick_on;
        int tick_off;
    };
    QVector<SourceNote> m_sources;
    QVector<smf::MidiEvent *> m_created_on;
    QVector<smf::MidiEvent *> m_created_off;
    int m_delta_tick;
    int m_delta_key;
    bool m_first_redo = true;
};

#endif // EDITCOMMANDS_H
