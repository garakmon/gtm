#pragma once
#ifndef TRACKROLL_H
#define TRACKROLL_H

#include <QObject>
#include <QGraphicsScene>
#include <memory>



class Song;

class TrackRoll : public QObject {
    Q_OBJECT

public:
    TrackRoll(QObject *parent = nullptr);

    QGraphicsScene *sceneRoll() { return &this->m_scene_roll; };

    void setSong(std::shared_ptr<Song> song) {
        this->m_active_song = song;
        this->drawTracks();
    }

private:
    void drawTracks();

private:
    // vert scrolls the same
    QGraphicsScene m_scene_roll;

    std::shared_ptr<Song> m_active_song;
};


#endif // TRACKROLL_H
