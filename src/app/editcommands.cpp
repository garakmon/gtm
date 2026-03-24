#include "app/editcommands.h"

#include "sound/song.h"

#include <QtGlobal>

#include <algorithm>



EditNote::EditNote(Song *song, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent), m_song(song) {
    Q_ASSERT(m_song != nullptr);
}



//////////////////////////////////////////////////////////////////////////////////////////



MoveNotes::MoveNotes(Song *song, const QVector<smf::MidiEvent *> &notes,
                     int delta_tick, int delta_key)
  : EditNote(song, QStringLiteral("Move Notes")),
    m_delta_tick(delta_tick), m_delta_key(delta_key) {
    m_states.reserve(notes.size());
    for (smf::MidiEvent *note_on : notes) {
        smf::MidiEvent *note_off = note_on->getLinkedEvent();
        Q_ASSERT(note_off != nullptr);

        NoteState state;
        state.note_on = note_on;
        state.note_off = note_off;
        state.orig_tick_on = note_on->tick;
        state.orig_tick_off = note_off->tick;
        state.orig_key = note_on->getKeyNumber();
        m_states.append(state);
    }
}

void MoveNotes::redo() {
    for (const NoteState &s : m_states) {
        int new_tick_on = std::max(0, s.orig_tick_on + m_delta_tick);
        int new_tick_off = std::max(0, s.orig_tick_off + m_delta_tick);
        int new_key = std::clamp(s.orig_key + m_delta_key, 0, 127);

        s.note_on->tick = new_tick_on;
        s.note_off->tick = new_tick_off;
        s.note_on->setKeyNumber(new_key);
        s.note_off->setKeyNumber(new_key);
    }

    if (m_delta_tick != 0) {
        m_song->rebuildAfterTickEdit();
    }
}

void MoveNotes::undo() {
    for (const NoteState &s : m_states) {
        s.note_on->tick = s.orig_tick_on;
        s.note_off->tick = s.orig_tick_off;
        s.note_on->setKeyNumber(s.orig_key);
        s.note_off->setKeyNumber(s.orig_key);
    }

    if (m_delta_tick != 0) {
        m_song->rebuildAfterTickEdit();
    }
}

bool MoveNotes::mergeWith(const QUndoCommand *other) {
    if (!other || other->id() != this->id()) {
        return false;
    }

    const MoveNotes *cmd = static_cast<const MoveNotes *>(other);
    if (cmd->m_song != m_song) {
        return false;
    }

    if (m_states.size() != cmd->m_states.size()) {
        return false;
    }

    // verify same note set
    for (int i = 0; i < m_states.size(); i++) {
        if (m_states[i].note_on != cmd->m_states[i].note_on) {
            return false;
        }
    }

    m_delta_tick += cmd->m_delta_tick;
    m_delta_key += cmd->m_delta_key;
    return true;
}

QVector<smf::MidiEvent *> MoveNotes::affectedNoteEvents() const {
    QVector<smf::MidiEvent *> events;
    events.reserve(m_states.size());

    for (const NoteState &state : m_states) {
        events.append(state.note_on);
    }

    return events;
}



//////////////////////////////////////////////////////////////////////////////////////////



ResizeNotes::ResizeNotes(Song *song, const QVector<smf::MidiEvent *> &notes,
                         int delta_tick, bool resize_end)
    : EditNote(song, QStringLiteral("Resize Notes")),
      m_delta_tick(delta_tick), m_resize_end(resize_end) {
    m_states.reserve(notes.size());
    for (smf::MidiEvent *note_on : notes) {
        smf::MidiEvent *note_off = note_on->getLinkedEvent();
        Q_ASSERT(note_off != nullptr);

        NoteState state;
        state.note_on = note_on;
        if (resize_end) {
            state.target = note_off;
            state.other = note_on;
        } else {
            state.target = note_on;
            state.other = note_off;
        }
        state.orig_tick = state.target->tick;
        m_states.append(state);
    }
}

void ResizeNotes::redo() {
    for (const NoteState &s : m_states) {
        int new_tick = s.orig_tick + m_delta_tick;

        // clamp so duration >= 1 and tick >= 0
        if (m_resize_end) {
            new_tick = std::max(new_tick, s.other->tick + 1);
        } else {
            new_tick = std::max(0, new_tick);
            new_tick = std::min(new_tick, s.other->tick - 1);
        }

        s.target->tick = new_tick;
    }

    m_song->rebuildAfterTickEdit();
}

void ResizeNotes::undo() {
    for (const NoteState &s : m_states) {
        s.target->tick = s.orig_tick;
    }

    m_song->rebuildAfterTickEdit();
}

bool ResizeNotes::mergeWith(const QUndoCommand *other) {
    if (!other || other->id() != this->id()) {
        return false;
    }

    const ResizeNotes *cmd = static_cast<const ResizeNotes *>(other);
    if (cmd->m_song != m_song) {
        return false;
    }

    if (m_resize_end != cmd->m_resize_end) {
        return false;
    }

    if (m_states.size() != cmd->m_states.size()) {
        return false;
    }

    for (int i = 0; i < m_states.size(); i++) {
        if (m_states[i].target != cmd->m_states[i].target) {
            return false;
        }
    }

    m_delta_tick += cmd->m_delta_tick;
    return true;
}

QVector<smf::MidiEvent *> ResizeNotes::affectedNoteEvents() const {
    QVector<smf::MidiEvent *> events;
    events.reserve(m_states.size());

    for (const NoteState &state : m_states) {
        events.append(state.note_on);
    }

    return events;
}



//////////////////////////////////////////////////////////////////////////////////////////



DeleteNotes::DeleteNotes(Song *song, const QVector<smf::MidiEvent *> &notes)
    : EditNote(song, QStringLiteral("Delete Notes")) {
    m_notes.reserve(notes.size());
    for (smf::MidiEvent *note_on : notes) {
        smf::MidiEvent *note_off = note_on->getLinkedEvent();
        Q_ASSERT(note_off != nullptr);

        DeletedNote dn;
        dn.note_on = note_on;
        dn.note_off = note_off;
        dn.on_tick = note_on->tick;
        dn.off_tick = note_off->tick;
        dn.track = note_on->track;
        m_notes.append(dn);
    }
}

void DeleteNotes::redo() {
    auto &song_notes = m_song->getNotes();

    for (DeletedNote &dn : m_notes) {
        // save original bytes on first redo
        if (m_first_redo) {
            dn.on_bytes.assign(dn.note_on->begin(), dn.note_on->end());
            dn.off_bytes.assign(dn.note_off->begin(), dn.note_off->end());
        }

        dn.note_on->unlinkEvent();
        dn.note_on->resize(0);
        dn.note_off->resize(0);

        // remove from song notes list
        for (int i = song_notes.size() - 1; i >= 0; i--) {
            if (song_notes[i].second == dn.note_on) {
                song_notes.removeAt(i);
                break;
            }
        }
    }

    m_first_redo = false;
    m_song->rebuildAfterDelete();
}

void DeleteNotes::undo() {
    auto &song_notes = m_song->getNotes();

    for (DeletedNote &dn : m_notes) {
        // restore bytes
        dn.note_on->assign(dn.on_bytes.begin(), dn.on_bytes.end());
        dn.note_off->assign(dn.off_bytes.begin(), dn.off_bytes.end());
        dn.note_on->tick = dn.on_tick;
        dn.note_off->tick = dn.off_tick;

        // re-link pair
        dn.note_on->linkEvent(dn.note_off);

        // re-add to song notes list
        song_notes.append({dn.track, dn.note_on});
    }

    m_song->rebuildAfterDelete();
}

QVector<smf::MidiEvent *> DeleteNotes::affectedNoteEvents() const {
    QVector<smf::MidiEvent *> events;
    events.reserve(m_notes.size());

    for (const DeletedNote &note : m_notes) {
        events.append(note.note_on);
    }

    return events;
}



//////////////////////////////////////////////////////////////////////////////////////////



// !TODO: verify redo-after-undo cannot append duplicate note pointers into m_notes
// (test: create -> undo -> redo repeatedly and check m_notes uniqueness)
CreateNotes::CreateNotes(Song *song, const QVector<NoteCreateSettings> &notes)
    : EditNote(song, QStringLiteral("Create Notes")),
      m_settings(notes) {}

void CreateNotes::redo() {
    auto &song_notes = m_song->getNotes();

    if (m_first_redo) {
        m_created_on.reserve(m_settings.size());
        m_created_off.reserve(m_settings.size());

        for (const NoteCreateSettings &s : m_settings) {
            smf::MidiEvent *on = m_song->addNoteOn(s.track, s.tick,
                                                    s.channel, s.key, s.velocity);
            smf::MidiEvent *off = m_song->addNoteOff(s.track, s.tick + s.duration,
                                                     s.channel, s.key);
            on->track = s.track;
            off->track = s.track;
            on->linkEvent(off);
            m_created_on.append(on);
            m_created_off.append(off);

            song_notes.append({s.track, on});

        }
        m_first_redo = false;
    } else {
        // re-redo: restore bytes from what were saved during undo
        for (int i = 0; i < m_settings.size(); i++) {
            const NoteCreateSettings &s = m_settings[i];
            smf::MidiEvent *on = m_created_on[i];
            smf::MidiEvent *off = m_created_off[i];

            on->makeNoteOn(s.channel, s.key, s.velocity);
            on->tick = s.tick;
            on->track = s.track;
            off->makeNoteOff(s.channel, s.key);
            off->tick = s.tick + s.duration;
            off->track = s.track;
            on->linkEvent(off);

            song_notes.append({s.track, on});

        }
    }

    m_song->rebuildAfterTickEdit();
}

void CreateNotes::undo() {
    auto &song_notes = m_song->getNotes();

    for (int i = 0; i < m_created_on.size(); i++) {
        smf::MidiEvent *on = m_created_on[i];
        smf::MidiEvent *off = m_created_off[i];

        on->unlinkEvent();
        on->resize(0);
        off->resize(0);

        for (int j = song_notes.size() - 1; j >= 0; j--) {
            if (song_notes[j].second == on) {
                song_notes.removeAt(j);
                break;
            }
        }
    }

    m_song->rebuildAfterDelete();
}

QVector<smf::MidiEvent *> CreateNotes::affectedNoteEvents() const {
    return m_created_on;
}



//////////////////////////////////////////////////////////////////////////////////////////



DuplicateNotes::DuplicateNotes(Song *song, const QVector<smf::MidiEvent *> &source_notes,
                               int delta_tick, int delta_key)
    : EditNote(song, QStringLiteral("Duplicate Notes")),
      m_delta_tick(delta_tick), m_delta_key(delta_key) {
    m_sources.reserve(source_notes.size());
    for (smf::MidiEvent *note_on : source_notes) {
        smf::MidiEvent *note_off = note_on->getLinkedEvent();
        Q_ASSERT(note_off != nullptr);

        SourceNote sn;
        sn.track = note_on->track;
        sn.channel = note_on->getChannel();
        sn.key = note_on->getKeyNumber();
        sn.velocity = note_on->getVelocity();
        sn.tick_on = note_on->tick;
        sn.tick_off = note_off->tick;
        m_sources.append(sn);
    }
}

void DuplicateNotes::redo() {
    auto &song_notes = m_song->getNotes();

    if (m_first_redo) {
        m_created_on.reserve(m_sources.size());
        m_created_off.reserve(m_sources.size());

        for (const SourceNote &sn : m_sources) {
            int new_key = std::clamp(sn.key + m_delta_key, 0, 127);
            int new_tick_on = std::max(0, sn.tick_on + m_delta_tick);
            int new_tick_off = std::max(0, sn.tick_off + m_delta_tick);

            smf::MidiEvent *on = m_song->addNoteOn(sn.track, new_tick_on,
                                                    sn.channel, new_key, sn.velocity);
            smf::MidiEvent *off = m_song->addNoteOff(sn.track, new_tick_off,
                                                     sn.channel, new_key);
            on->linkEvent(off);
            m_created_on.append(on);
            m_created_off.append(off);

            song_notes.append({sn.track, on});
        }
        m_first_redo = false;
    } else {
        // !TODO: verify redo-after-undo cannot append duplicate note pointers into m_notes.
        // stress test by doing duplicate -> undo -> redo repeatedly and assert m_notes uniqueness.
        for (int i = 0; i < m_sources.size(); i++) {
            const SourceNote &sn = m_sources[i];
            smf::MidiEvent *on = m_created_on[i];
            smf::MidiEvent *off = m_created_off[i];
            int new_key = std::clamp(sn.key + m_delta_key, 0, 127);
            int new_tick_on = std::max(0, sn.tick_on + m_delta_tick);
            int new_tick_off = std::max(0, sn.tick_off + m_delta_tick);

            on->makeNoteOn(sn.channel, new_key, sn.velocity);
            on->tick = new_tick_on;
            off->makeNoteOff(sn.channel, new_key);
            off->tick = new_tick_off;
            on->linkEvent(off);

            song_notes.append({sn.track, on});
        }
    }

    m_song->rebuildAfterTickEdit();
}

void DuplicateNotes::undo() {
    auto &song_notes = m_song->getNotes();

    for (int i = 0; i < m_created_on.size(); i++) {
        smf::MidiEvent *on = m_created_on[i];
        smf::MidiEvent *off = m_created_off[i];

        on->unlinkEvent();
        on->resize(0);
        off->resize(0);

        for (int j = song_notes.size() - 1; j >= 0; j--) {
            if (song_notes[j].second == on) {
                song_notes.removeAt(j);
                break;
            }
        }
    }

    m_song->rebuildAfterDelete();
}

QVector<smf::MidiEvent *> DuplicateNotes::affectedNoteEvents() const {
    return m_created_on;
}
