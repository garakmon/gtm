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
    this->m_scene_track_list.clear();
    this->m_scene_roll.clear();

    int track_num = 0;
    int display_row = 0;
    for (auto track : this->m_active_song->tracks()) {
        if (this->m_active_song->isMetaTrack(track_num)) {
            track_num++;
            continue;
        }

        GraphicsTrackItem *track_item = new GraphicsTrackItem(track_num, display_row);
        this->m_scene_track_list.addItem(track_item);
        this->m_scene_roll.addItem(new GraphicsTrackRollManager(track, track_item));

        track_num++;
        display_row++;
    }
}
