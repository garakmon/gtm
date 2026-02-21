#pragma once
#ifndef GRAPHICSTRACKITEM_H
#define GRAPHICSTRACKITEM_H

#include <QGraphicsObject>

#include <cstdint>



using TrackEventViewMask = uint32_t;

enum TrackEventViewFlag : TrackEventViewMask {
    TrackEventView_Program      = 1 << 0,
    TrackEventView_Volume       = 1 << 1,
    TrackEventView_Expression   = 1 << 2,
    TrackEventView_Pan          = 1 << 3,
    TrackEventView_Pitch        = 1 << 4,
    TrackEventView_ControlOther = 1 << 5,
};

constexpr TrackEventViewMask TrackEventView_All =
    TrackEventView_Program | TrackEventView_Volume | TrackEventView_Expression |
    TrackEventView_Pan | TrackEventView_Pitch | TrackEventView_ControlOther;

class GraphicsTrackMeterItem;
class GraphicsTrackButtonItem;
struct VoiceGroup;
struct KeysplitTable;
struct Instrument;
namespace smf { class MidiEventList; }
namespace smf { class MidiEvent; }

//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsTrackItem is the ui object for one track row in the track roll.
/// It owns the row’s visual state and child control
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsTrackItem : public QGraphicsObject {
    Q_OBJECT

public:
    GraphicsTrackItem(int track, int row, QGraphicsItem *parent = nullptr);
    ~GraphicsTrackItem() override = default;

    // disable move and copy
    GraphicsTrackItem(const GraphicsTrackItem &) = delete;
    GraphicsTrackItem &operator=(const GraphicsTrackItem &) = delete;
    GraphicsTrackItem(GraphicsTrackItem &&) = delete;
    GraphicsTrackItem &operator=(GraphicsTrackItem &&) = delete;

    // graphicsobject drawing
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    // content and playback display
    void setPlayingInfo(const QString &voiceType);
    void clearPlayingInfo();
    void setMeterLevels(float left, float right);

    // control state
    void setMuted(bool muted);
    void setSoloed(bool soloed);
    void buttonToggled(bool isMuteButton, bool checked);
    void setControlsEnabled(bool enabled);

    // layout
    void setYPosition(int y);
    void setExpanded(bool expanded);
    int totalHeight() const;

    // basic accessors
    QColor color() { return this->m_color; }
    QColor colorLight() const { return m_color_light; }
    int track() { return this->m_track; }
    int row() { return this->m_row; }
    bool isMuted() const { return m_muted; }
    bool isSoloed() const;
    bool isExpanded() const { return m_expanded; }

private:
    // internal ui refresh
    void updateMuteButton();
    void updateSoloButton();

signals:
    void muteToggled(int channel, bool muted);
    void soloToggled(int channel, bool soloed);
    void trackClicked(int track);

private:
    // track identity and layout
    int m_track;
    int m_row;
    int m_y_position = 0;
    bool m_expanded = false;

    // track visuals
    QColor m_color;
    QColor m_color_light;

    // current ui state
    bool m_muted = false;
    QString m_playing_voice_type;
    QString m_last_voice_type;

    // child graphics items
    GraphicsTrackButtonItem *m_mute_button = nullptr;
    GraphicsTrackButtonItem *m_solo_button = nullptr;
    GraphicsTrackMeterItem *m_meter_item = nullptr;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsTrackMetaEventItem is the per-event marker item drawn on a track row for
/// track-specific meta/control events such as program changes and CC changes.
///
//////////////////////////////////////////////////////////////////////////////////////////
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

    GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track,
                               const VoiceGroup *song_voicegroup = nullptr,
                               const QMap<QString, VoiceGroup> *all_voicegroups = nullptr,
                               const QMap<QString, KeysplitTable> *keysplits = nullptr);
    ~GraphicsTrackMetaEventItem() override = default;

    // disable copy + move
    GraphicsTrackMetaEventItem(const GraphicsTrackMetaEventItem &) = delete;
    GraphicsTrackMetaEventItem &operator=(const GraphicsTrackMetaEventItem &) = delete;
    GraphicsTrackMetaEventItem(GraphicsTrackMetaEventItem &&) = delete;
    GraphicsTrackMetaEventItem &operator=(GraphicsTrackMetaEventItem &&) = delete;

    // graphics drawing
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QPainterPath shape() const override;

    // layout and data
    void setBucketCount(int count) { m_bucket_count = count; }
    void setBucketOverflow(int overflow) { m_bucket_overflow = overflow; }
    void setLane(int lane) { m_lane = lane; }
    smf::MidiEvent *event() const { return m_event; }
    EventType eventType() const { return m_type; }

private:
    // input handling
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    // event helpers
    static EventType detectType(const smf::MidiEvent *event);
    QColor eventColor() const;
    QString hoverText() const;
    QString buildProgramHoverText() const;

private:
    // source event
    smf::MidiEvent *m_event = nullptr;
    EventType m_type = EventType::ControlOther;

    const VoiceGroup *m_song_voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *m_all_voicegroups = nullptr;
    const QMap<QString, KeysplitTable> *m_keysplit_tables = nullptr;

    // parent context
    GraphicsTrackItem *m_parent_track = nullptr;

    // cached display data
    QString m_program_hover_text;
    qreal m_hover_width = 84.0;

    // layout and interaction state
    bool m_hovered = false;
    int m_bucket_count = 1;
    int m_bucket_overflow = 0;
    int m_lane = 0;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// GraphicsTrackRollManager manages all meta events in a track.
///
//////////////////////////////////////////////////////////////////////////////////////////
class GraphicsTrackRollManager : public QGraphicsItem {
public:
    struct CCPoint { int tick; int value; };

    GraphicsTrackRollManager(smf::MidiEventList *event_list,
                             GraphicsTrackItem *parent_track,
                             int initial_vol, int initial_pan,
                             int initial_expr, int initial_bend,
                             const VoiceGroup *song_voicegroup = nullptr,
                             const QMap<QString, VoiceGroup> *all_voicegroups = nullptr,
                             const QMap<QString, KeysplitTable> *keysplits = nullptr);
    ~GraphicsTrackRollManager() override = default;

    GraphicsTrackRollManager(const GraphicsTrackRollManager &) = delete;
    GraphicsTrackRollManager &operator=(const GraphicsTrackRollManager &) = delete;
    GraphicsTrackRollManager(GraphicsTrackRollManager &&) = delete;
    GraphicsTrackRollManager &operator=(GraphicsTrackRollManager &&) = delete;

    // qt overrides
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    // view state
    void setExpanded(bool expanded);
    void setEventViewMask(TrackEventViewMask mask);

private:
    // graph construction
    void buildStepGraphData(int initial_vol, int initial_pan,
                            int initial_expr, int initial_bend);

    // paint helpers
    void paintMiniStepGraphs(QPainter *painter, const QStyleOptionGraphicsItem *option);
    void paintExpandedLanes(QPainter *painter, const QStyleOptionGraphicsItem *option);
    void paintStepLine(QPainter *painter, const QVector<CCPoint> &data,
                       Qt::PenStyle pen_style, const QRectF &rect,
                       int val_min, int val_max);

    // visibility and layout helpers
    bool isVisibleType(GraphicsTrackMetaEventItem::EventType type) const;
    int laneForType(GraphicsTrackMetaEventItem::EventType type) const;
    qreal markerYForLane(int lane) const;

private:
    // source data
    smf::MidiEventList *m_event_list = nullptr;
    const VoiceGroup *m_song_voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *m_all_voicegroups = nullptr;
    const QMap<QString, KeysplitTable> *m_keysplit_tables = nullptr;

    // parent context
    GraphicsTrackItem *m_parent_track = nullptr;

    // layout and filter state
    int m_width = 0;
    bool m_expanded = false;
    TrackEventViewMask m_event_view_mask = TrackEventView_All;

    // child items
    QList<GraphicsTrackMetaEventItem *> m_items;

    // cached controller graph data
    QVector<CCPoint> m_cc_volume;
    QVector<CCPoint> m_cc_expression;
    QVector<CCPoint> m_cc_pan;
    QVector<CCPoint> m_cc_pitch;
};

#endif // GRAPHICSTRACKITEM_H
