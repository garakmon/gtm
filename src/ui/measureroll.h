#pragma once

#include <QGraphicsScene>
#include <QObject>
#include <QVector>

#include <memory>



class QGraphicsLineItem;
class QGraphicsPolygonItem;
class GraphicsMetaEventItem;

class Song;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// MeasureRoll owns the timeline strip above the piano roll and track roll.
/// It builds and maintains the measure/beat grid, timestamps, and song meta event
/// markers, and it also owns the playhead indicator shown in that strip.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MeasureRoll : public QObject {
    Q_OBJECT

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

private:
    // drawing helpers
    void drawMeasures();
    void drawMetaEvents();

private:
    std::shared_ptr<Song> m_active_song;

    // scene state
    QGraphicsScene m_scene_measures;

    // child items
    QGraphicsLineItem *m_playhead_line = nullptr;
    QGraphicsPolygonItem *m_playhead_arrow = nullptr;
    QVector<GraphicsMetaEventItem *> m_meta_event_items;

    // playback state
    int m_current_tick = 0;
    int m_last_playhead_x = -1;
};
