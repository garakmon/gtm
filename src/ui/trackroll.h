#pragma once
#ifndef TRACKROLL_H
#define TRACKROLL_H

#include <QObject>
#include <QGraphicsScene>
#include <QMap>
#include <memory>



class Song;
class GraphicsTrackItem;

class TrackRoll : public QObject {
    Q_OBJECT

public:
    TrackRoll(QObject *parent = nullptr);

    QGraphicsScene *sceneRoll() { return &this->m_scene_roll; };
    QGraphicsScene *sceneTracks() { return &this->m_scene_track_list; }

    void setSong(std::shared_ptr<Song> song) {
        this->m_scene_roll.clear();
        this->m_scene_track_list.clear();
        this->m_track_items.clear();

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

signals:
    void trackMuteToggled(int channel, bool muted);
    void trackSoloToggled(int channel, bool soloed);

private:
    void drawTracks();

private:
    // vert scrolls the same
    QGraphicsScene m_scene_roll;
    QGraphicsScene m_scene_track_list;

    std::shared_ptr<Song> m_active_song;

    // Map channel to track item for playback info display
    QMap<int, GraphicsTrackItem *> m_track_items;
};


#endif // TRACKROLL_H
