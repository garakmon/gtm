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
