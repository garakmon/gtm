#pragma once
#ifndef MEASUREROLL_H
#define MEASUREROLL_H

#include <memory>
#include <QObject>
#include <QGraphicsScene>



class Song;
class MeasureRoll : public QObject {
    Q_OBJECT
public:
    MeasureRoll(QObject *parent = nullptr);

    QGraphicsScene *sceneMeasures() { return &this->m_scene_measures; };

    void setSong(std::shared_ptr<Song> song) {
        this->m_active_song = song;
        drawMeasures();
    }

    int tick() { return this->m_current_tick; }
    // TODO: advance() move tick, Controller has tick info to scroll all views
    bool advance();
    void setTick(int tick) { this->m_current_tick = tick; }

    // QGraphicsItem *marker() { return this->m_marker; } // ensure visible (for scrolling?)

private:
    void drawMeasures();

private:
    QGraphicsScene m_scene_measures;

    int m_current_tick = 0;
    std::shared_ptr<Song> m_active_song;
};

#endif // MEASUREROLL_H
