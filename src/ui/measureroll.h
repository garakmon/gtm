#pragma once
#ifndef MEASUREROLL_H
#define MEASUREROLL_H

#include "ui/graphicsselectionitems.h"

#include <QGraphicsScene>
#include <QObject>
#include <QVector>

#include <memory>



class QGraphicsLineItem;
class QGraphicsPolygonItem;
class GraphicsMetaEventItem;
class MeasureRoll;
class QGraphicsSceneMouseEvent;
class Song;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// MeasureRollScene owns time-range selection gestures that begin in the measure roll.
/// It translates scene mouse input into the higher-level selection behavior on
/// MeasureRoll, so the Controller does not need to micromanage that drag state.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MeasureRollScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit MeasureRollScene(QObject *parent = nullptr);
    void setMeasureRoll(MeasureRoll *measure_roll);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    MeasureRoll *m_measure_roll = nullptr;
};

//////////////////////////////////////////////////////////////////////////////////////////
///
/// MeasureRoll owns the timeline strip above the piano roll and track roll.
/// It builds and maintains the measure/beat grid, timestamps, and song meta event
/// markers, and it also owns the playhead indicator shown in that strip.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MeasureRoll : public QObject {
    Q_OBJECT
    friend class MeasureRollScene;

public:
    MeasureRoll(QObject *parent = nullptr);
    ~MeasureRoll() override = default;

    MeasureRoll(const MeasureRoll &) = delete;
    MeasureRoll &operator=(const MeasureRoll &) = delete;
    MeasureRoll(MeasureRoll &&) = delete;
    MeasureRoll &operator=(MeasureRoll &&) = delete;

    // scene access
    QGraphicsScene *sceneMeasures();

    void setSong(std::shared_ptr<Song> song);

    int tick();
    bool advance();
    void setTick(int tick);

    // playhead display
    void createPlaybackGuide();
    void updatePlaybackGuide(int tick);

    // time-range selection
    void setTimeSelectEnabled(bool enabled);
    bool isTimeSelectEnabled() const;

signals:
    void onTimeRangeSelected(int start_tick, int end_tick);
    void onTimeSelectionCleared();

private:
    struct TimeSelectState {
        bool pressed = false;
        bool dragging = false;
        qreal start_scene_x = 0.0;
        qreal current_scene_x = 0.0;
    };

    // drawing helpers
    void resetScene();
    void drawMeasures();
    void drawMetaEvents();

    // time selection
    void beginTimeSelect(const QPointF &scene_pos);
    void updateTimeSelect(const QPointF &scene_pos);
    void finishTimeSelect();
    void cancelTimeSelect();
    void clearTimeSelect();
    QPair<int, int> currentTimeSelectTickRange() const;

private:
    std::shared_ptr<Song> m_active_song;

    // scene state
    MeasureRollScene m_scene_measures;
    GraphicsTimeRangeSelectItem m_time_select_preview;
    TimeSelectState m_time_select;
    bool m_time_select_enabled = false;

    // child items
    QGraphicsLineItem *m_playhead_line = nullptr;
    QGraphicsPolygonItem *m_playhead_arrow = nullptr;
    QVector<GraphicsMetaEventItem *> m_meta_event_items;

    // playback state
    int m_current_tick = 0;
    int m_last_playhead_x = -1;
};

#endif // MEASUREROLL_H
