#pragma once

#ifndef GRAPHICSTRACKITEM_H
#define GRAPHICSTRACKITEM_H

// Track item:
//   track color
//   track number (0 indexed)
//   number of events
//   mute / unmute track buttons
//   instrument
//   volume
//   pan
//   

#include <QGraphicsObject>
#include <cstdint>

using TrackEventViewMask = uint32_t;

enum TrackEventViewFlag : TrackEventViewMask {
    TrackEventView_Program      = 1u << 0,
    TrackEventView_Volume       = 1u << 1,
    TrackEventView_Expression   = 1u << 2,
    TrackEventView_Pan          = 1u << 3,
    TrackEventView_Pitch        = 1u << 4,
    TrackEventView_ControlOther = 1u << 5,
};

constexpr TrackEventViewMask kTrackEventView_All =
    TrackEventView_Program | TrackEventView_Volume | TrackEventView_Expression |
    TrackEventView_Pan | TrackEventView_Pitch | TrackEventView_ControlOther;

class GraphicsTrackMeterItem;
class GraphicsTrackButtonItem;


class GraphicsScoreItem;
namespace smf { class MidiEventList; }
namespace smf { class MidiEvent; }

class GraphicsTrackItem : public QGraphicsObject {
    Q_OBJECT

public:
    GraphicsTrackItem(int track, int row, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    QColor color() { return this->m_color; }
    QColor colorLight() const { return m_color_light; }
    int track() { return this->m_track; }
    int row() { return this->m_row; }
    bool isMuted() const { return m_muted; }
    bool isSoloed() const;

    void addItem(GraphicsScoreItem *item);

    // Update the current playing instrument info for display
    void setPlayingInfo(const QString &voiceType);
    void clearPlayingInfo();
    void setMuted(bool muted);
    void setSoloed(bool soloed);
    void setMeterLevels(float left, float right);
    void buttonToggled(bool isMuteButton, bool checked);
    void setControlsEnabled(bool enabled);

    void setYPosition(int y);
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    int totalHeight() const;

signals:
    void muteToggled(int channel, bool muted);
    void soloToggled(int channel, bool soloed);
    void trackClicked(int track);

private:
    QColor m_color;
    QColor m_color_light;
    int m_track;
    int m_row;
    bool m_muted = false;

    GraphicsTrackButtonItem *m_mute_button = nullptr;
    GraphicsTrackButtonItem *m_solo_button = nullptr;
    GraphicsTrackMeterItem *m_meter_item = nullptr;

    QList<GraphicsScoreItem *> m_score_items;

    // Current playback info display
    QString m_playing_voice_type;
    QString m_last_voice_type;

    int m_y_position = 0;
    bool m_expanded = false;

    void updateMuteButton();
    void updateSoloButton();
};



///
/// Meta Items
///
class GraphicsTrackMetaEventItem : public QGraphicsItem {
public:
    enum class EventType {
        Program,
        Volume,
        Pan,
        Expression,
        Pitch,
        ControlOther
    };

    GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QPainterPath shape() const override;

    void setBucketCount(int count) { m_bucket_count = count; }
    void setBucketOverflow(int overflow) { m_bucket_overflow = overflow; }
    void setLane(int lane) { m_lane = lane; }
    smf::MidiEvent *event() const { return m_event; }
    EventType eventType() const { return m_type; }

private:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    static EventType detectType(const smf::MidiEvent *event);
    QColor eventColor() const;
    QString hoverText() const;

    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEvent *m_event = nullptr;
    EventType m_type = EventType::ControlOther;
    bool m_hovered = false;
    int m_bucket_count = 1;
    int m_bucket_overflow = 0;
    int m_lane = 0;
};



///
/// GraphicsTrackRollManager manages all meta events in a track
///
class GraphicsTrackRollManager : public QGraphicsItem {
public:
    struct CCPoint { int tick; int value; };

    GraphicsTrackRollManager(smf::MidiEventList *event_list, GraphicsTrackItem *parent_track,
                             int initial_vol, int initial_pan, int initial_expr, int initial_bend);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setExpanded(bool expanded);
    void setEventViewMask(TrackEventViewMask mask);

private:
    void buildStepGraphData(int initial_vol, int initial_pan, int initial_expr, int initial_bend);
    void paintMiniStepGraphs(QPainter *painter, const QStyleOptionGraphicsItem *option);
    void paintExpandedLanes(QPainter *painter, const QStyleOptionGraphicsItem *option);
    void paintStepLine(QPainter *painter, const QVector<CCPoint> &data,
                       Qt::PenStyle pen_style, const QRectF &rect, int val_min, int val_max);
    bool isVisibleType(GraphicsTrackMetaEventItem::EventType type) const;
    int laneForType(GraphicsTrackMetaEventItem::EventType type) const;
    qreal markerYForLane(int lane) const;

    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEventList *m_event_list = nullptr;
    int m_width = 0;
    bool m_expanded = false;
    TrackEventViewMask m_event_view_mask = kTrackEventView_All;
    QList<GraphicsTrackMetaEventItem *> m_items;
    QVector<CCPoint> m_cc_volume;
    QVector<CCPoint> m_cc_expression;
    QVector<CCPoint> m_cc_pan;
    QVector<CCPoint> m_cc_pitch;
};

#endif // GRAPHICSTRACKITEM_H
