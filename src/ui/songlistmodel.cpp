#include "songlistmodel.h"

#include <QColor>

#include "../app/project.h"



int SongListModel::rowCount(const QModelIndex &parent) const {
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
