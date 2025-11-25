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
    this->m_track_items.clear();

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

        // Connect mute signal
        connect(track_item, &GraphicsTrackItem::muteToggled, this, &TrackRoll::trackMuteToggled);

        // Store by display row (which maps to MIDI channel for typical GBA music)
        m_track_items[display_row] = track_item;

        track_num++;
        display_row++;
    }
}

void TrackRoll::setTrackPlayingInfo(int channel, const QString &instrument, const QString &voiceType) {
    // Channel typically maps to display row for GBA music
    if (m_track_items.contains(channel)) {
        m_track_items[channel]->setPlayingInfo(instrument, voiceType);
    }
}

void TrackRoll::clearAllPlayingInfo() {
    for (auto item : m_track_items) {
        item->clearPlayingInfo();
    }
}
