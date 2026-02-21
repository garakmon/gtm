#include "ui/songlistmodel.h"

#include "app/project.h"

#include <QColor>
#include <QIcon>



int SongListModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return this->m_project->getNumSongsInTable();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= this->m_project->getNumSongsInTable()) {
        return QVariant();
    }

    const QString &title = this->m_project->getSongTitleAt(index.row());
    const SongEntry &entry = m_project->getSongEntryByTitle(title);

    if (role == Qt::DisplayRole) {
        return QString("[%1] ").arg(index.row(), 4, 10, QLatin1Char('0')) + title;
    }

    if (role == Qt::UserRole) {
        return title;
    }

    if (role == Qt::ForegroundRole) {
        // files without a rule in midi.cfg cannot be played (at least for now)
        // !TODO: how do I just fully disable an item?
        if (entry.midifile.isEmpty()) {
            return QColor(0x808080);
        }
        // else if (this->m_project->getSong().isEmpty()) {
        //     // empty midi file?
        //     return QColor(0x009922);
        // }
        return QColor(0xffffff);
    }

    if (role == Qt::DecorationRole) {
        static const QIcon s_song_icon_default(":/icons/song.svg");
        static const QIcon s_song_icon_loaded(":/icons/song-loaded.svg");
        static const QIcon s_song_icon_editing(":/icons/song-editing.svg");
        static const QIcon s_song_icon_unsaved(":/icons/song-unsaved.svg");

        const bool loaded = m_project->songLoaded(title);
        if (loaded) {
            auto song = m_project->getSong(title);
            auto active = m_project->activeSong();
            if (song == active) {
                return s_song_icon_editing;
            }
            else if (!song->isClean()) {
                return s_song_icon_unsaved;
            }
            return s_song_icon_loaded;
        }

        return s_song_icon_default;
    }

    return QVariant();
}

Qt::ItemFlags SongListModel::flags(const QModelIndex &index) const {
    if (!index.isValid() || index.row() >= this->m_project->getNumSongsInTable()) {
        return Qt::NoItemFlags;
    }

    const QString &title = this->m_project->getSongTitleAt(index.row());
    const SongEntry &entry = m_project->getSongEntryByTitle(title);

    if (entry.midifile.isEmpty()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void SongListModel::refreshAll() {
    const int rows = this->rowCount();
    if (rows <= 0) return;
    const QModelIndex first = this->index(0, 0);
    const QModelIndex last = this->index(rows - 1, 0);
    emit dataChanged(first, last, {Qt::DecorationRole});
}
