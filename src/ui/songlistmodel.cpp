#include "ui/songlistmodel.h"

#include "app/project.h"

#include <QGuiApplication>
#include <QFont>
#include <QIcon>
#include <QPalette>



int SongListModel::rowCount(const QModelIndex &) const {
    if (!m_project) return 0;
    return m_project->getNumSongsInTable();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const {
    if (!m_project || !index.isValid() || index.row() >= m_project->getNumSongsInTable()) {
        return QVariant();
    }

    // handle the lightweight text roles first
    if (role == Qt::DisplayRole || role == Qt::UserRole) {
        QString title = m_project->getSongTitleAt(index.row());
        if (role == Qt::DisplayRole) {
            return QString("[%1] ").arg(index.row(), 4, 10, QLatin1Char('0')) + title;
        }
        return title;
    }

    QString title = m_project->getSongTitleAt(index.row());
    SongEntry &entry = m_project->getSongEntryByTitle(title);

    if (role == Qt::ForegroundRole) {
        // show missing midi entries as disabled text
        if (entry.midifile.isEmpty()) {
            return QGuiApplication::palette().color(QPalette::Disabled, QPalette::Text);
        }
        return QVariant();
    }

    if (role == Qt::FontRole) {
        bool unsaved = m_project->isSongUnsaved(title);
        if (!unsaved && m_project->songLoaded(title)) {
            auto song = m_project->getSong(title);
            unsaved = song && !song->isClean();
        }
        if (unsaved) {
            QFont font = QGuiApplication::font();
            font.setItalic(true);
            return font;
        }
        return QVariant();
    }

    if (role == Qt::DecorationRole) {
        // choose an icon based on the song state in memory
        static const QIcon s_song_icon_default(":/icons/song.svg");
        static const QIcon s_song_icon_loaded(":/icons/song-loaded.svg");
        static const QIcon s_song_icon_editing(":/icons/song-editing.svg");
        static const QIcon s_song_icon_unsaved(":/icons/song-unsaved.svg");

        const bool loaded = m_project->songLoaded(title);
        const bool unsaved = m_project->isSongUnsaved(title);
        if (loaded) {
            auto song = m_project->getSong(title);
            auto active = m_project->activeSong();
            if (song == active) {
                return s_song_icon_editing;
            }
            else if (unsaved || (song && !song->isClean())) {
                return s_song_icon_unsaved;
            }
            return s_song_icon_loaded;
        }

        return s_song_icon_default;
    }

    return QVariant();
}

Qt::ItemFlags SongListModel::flags(const QModelIndex &index) const {
    if (!m_project || !index.isValid() || index.row() >= m_project->getNumSongsInTable()) {
        return Qt::NoItemFlags;
    }

    QString title = m_project->getSongTitleAt(index.row());
    SongEntry &entry = m_project->getSongEntryByTitle(title);

    if (entry.midifile.isEmpty()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void SongListModel::refreshAll() {
    if (!m_project) return;

    const int rows = this->rowCount();
    if (rows <= 0) return;

    const QModelIndex first = this->index(0, 0);
    const QModelIndex last = this->index(rows - 1, 0);
    emit dataChanged(first, last);
}
