#pragma once
#ifndef TRACKROLL_H
#define TRACKROLL_H

#include <QObject>
#include <QGraphicsScene>
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

        this->m_active_song = song;
        this->drawTracks();
    }

    GraphicsTrackItem *addTrack();

private:
    void drawTracks();

private:
    // vert scrolls the same
    QGraphicsScene m_scene_roll;
    QGraphicsScene m_scene_track_list;

    std::shared_ptr<Song> m_active_song;
};


#endif // TRACKROLL_H
