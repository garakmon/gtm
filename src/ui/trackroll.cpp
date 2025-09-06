#include "trackroll.h"

#include <QGraphicsView>
#include <QEvent>
#include <QWheelEvent>

#include "graphicstrackitem.h"
#include "song.h"
#include "constants.h"
#include "colors.h"
#include "util.h"



TrackRoll::TrackRoll(QObject *parent) : QObject(parent) {
    this->m_scene_roll.setParent(this);
}

void TrackRoll::drawTracks() {
    // 
    this->m_scene_roll.clear();

    int track_num = 0;
    for (auto track : this->m_active_song->tracks()) {
        // TODO m_tracks
        this->m_scene_roll.addItem(new GraphicsTrackItem(track_num));
        track_num++;
    }

    // this->ui->view_tracklist->setSceneRect(this->m_scene_roll->itemsBoundingRect());
    // this->ui->view_tracklist->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}
