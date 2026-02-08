#pragma once
#ifndef TRACKROLL_H
#define TRACKROLL_H

#include <QObject>
#include <QGraphicsScene>
#include <QMap>
#include <memory>

#include "graphicstrackitem.h"


class Song;
class GraphicsTrackItem;
class GraphicsTrackRollManager;

class TrackRoll : public QObject {
    Q_OBJECT

public:
    enum class TrackEventPreset {
        All,
        Mix,
        Timbre,
        Other,
        Custom
    };

    TrackRoll(QObject *parent = nullptr);

    QGraphicsScene *sceneRoll() { return &this->m_scene_roll; };
    QGraphicsScene *sceneTracks() { return &this->m_scene_track_list; }

    void setSong(std::shared_ptr<Song> song) {
        this->m_scene_roll.clear();
        this->m_scene_track_list.clear();
        this->m_track_items.clear();
        this->m_roll_managers.clear();
        this->m_expanded_row = -1;

        this->m_active_song = song;
        this->drawTracks();
    }

    GraphicsTrackItem *addTrack();

    // Update track display with current instrument info (called during playback)
    void setTrackPlayingInfo(int channel, const QString &voiceType);
    void clearAllPlayingInfo();
    void setTrackMuted(int channel, bool muted);
    void setTrackSoloed(int channel, bool soloed);
    void setTrackMeterLevels(int channel, float left, float right);
    void clearAllMeters();
    void clearAllSoloed();
    bool hasSoloed() const;
    void setTracksInteractive(bool enabled);
    void toggleTrackExpansion(int display_row);
    void setEventViewMask(TrackEventViewMask mask);
    TrackEventViewMask eventViewMask() const { return m_event_view_mask; }
    void setEventPreset(TrackEventPreset preset);
    TrackEventPreset eventPreset() const { return m_event_preset; }

signals:
    void trackMuteToggled(int channel, bool muted);
    void trackSoloToggled(int channel, bool soloed);
    void trackSelected(int track);
    void layoutChanged();

private slots:
    void onTrackClicked(int track);

private:
    void drawTracks();
    void relayout();

    // vert scrolls the same
    QGraphicsScene m_scene_roll;
    QGraphicsScene m_scene_track_list;

    std::shared_ptr<Song> m_active_song;

    // Map channel to track item for playback info display
    QMap<int, GraphicsTrackItem *> m_track_items;
    QMap<int, GraphicsTrackRollManager *> m_roll_managers;
    int m_expanded_row = -1;
    TrackEventViewMask m_event_view_mask = kTrackEventView_All;
    TrackEventPreset m_event_preset = TrackEventPreset::All;
};


#endif // TRACKROLL_H
