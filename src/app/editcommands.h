#pragma once
#ifndef EDITCOMMANDS_H
#define EDITCOMMANDS_H

#include <QString>
#include <QUndoCommand>



enum CommandId {
    ID_EditNote = 0,
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

    void undo() override;
    void redo() override;

    bool mergeWith(const QUndoCommand *command) override;
    int id() const override { return CommandId::ID_PaintMetatile; }

protected:
    Song *m_song = nullptr;
};

#endif // EDITCOMMANDS_H
