#include "app/editcommands.h"

#include "sound/song.h"

#include <QtGlobal>



EditNote::EditNote(Song *song, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_song(song) {
    Q_ASSERT(m_song != nullptr);
}

void EditNote::redo() {
    QUndoCommand::redo();

    // check data

    // do edit work
}

void EditNote::undo() {
    // check data

    // do undo edit work

    QUndoCommand::undo();
}

bool EditNote::mergeWith(const QUndoCommand *command) {
    const EditNote *other = static_cast<const EditNote *>(command);

    // make sure operating on same song

    // check command id?

    // comnine end result data (merge down)
    // this->end = other->end

    return true;
}
