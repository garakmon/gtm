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
#include <QGraphicsProxyWidget>

class QPushButton;
class CenteredStereoMeter;


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

signals:
    void muteToggled(int channel, bool muted);
    void soloToggled(int channel, bool soloed);

private:
    QColor m_color;
    QColor m_color_light;
    int m_track;
    int m_row;
    bool m_muted = false;

    QGraphicsProxyWidget *m_mute_proxy = nullptr;
    QPushButton *m_mute_button = nullptr;
    QGraphicsProxyWidget *m_solo_proxy = nullptr;
    QPushButton *m_solo_button = nullptr;
    QGraphicsProxyWidget *m_meter_proxy = nullptr;
    CenteredStereoMeter *m_meter_widget = nullptr;

    QList<GraphicsScoreItem *> m_score_items;

    // Current playback info display
    QString m_playing_voice_type;

    void updateMuteButton();
    void updateSoloButton();
};



///
/// Meta Items
///
class GraphicsTrackMetaEventItem : public QGraphicsItem {
    //
public:
    GraphicsTrackMetaEventItem(smf::MidiEvent *event, GraphicsTrackItem *parent_track);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEvent *m_event = nullptr;
    QString m_label;
};



///
/// GraphicsTrackRollManager manages all meta events in a track
///
class GraphicsTrackRollManager : public QGraphicsItem {
    //
public:
    GraphicsTrackRollManager(smf::MidiEventList *event_list, GraphicsTrackItem *parent_track);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    GraphicsTrackItem *m_parent_track = nullptr;
    smf::MidiEventList *m_event_list = nullptr;
    int m_width = 0;
};

#endif // GRAPHICSTRACKITEM_H
