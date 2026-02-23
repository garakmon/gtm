#pragma once
#ifndef TRACKROLL_H
#define TRACKROLL_H

#include "ui/graphicstrackitem.h"

#include <QGraphicsScene>
#include <QMap>
#include <QObject>

#include <memory>



class Song;
class GraphicsTrackItem;
class GraphicsTrackRollManager;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// TrackRoll owns the two scenes used for the track list and the track event roll.
/// It builds and manages the GraphicsTrackItem rows and their associated meta events.
///
//////////////////////////////////////////////////////////////////////////////////////////
class TrackRoll : public QObject {
    Q_OBJECT

public:
    enum class TrackEventPreset {
        All,
        Mix,
        Timbre,
        Other,
        Custom
    };

    TrackRoll(QObject *parent = nullptr);
    ~TrackRoll() override = default;

    // disable copy and move
    TrackRoll(const TrackRoll &) = delete;
    TrackRoll &operator=(const TrackRoll &) = delete;
    TrackRoll(TrackRoll &&) = delete;
    TrackRoll &operator=(TrackRoll &&) = delete;

    // scene getters
    QGraphicsScene *sceneRoll() { return &m_scene_roll; }
    QGraphicsScene *sceneTracks() { return &m_scene_track_list; }

    void setSong(std::shared_ptr<Song> song);

    // playback display
    void setTrackPlayingInfo(int channel, const QString &voiceType);
    void clearAllPlayingInfo();
    void setTrackMeterLevels(int channel, float left, float right);
    void clearAllMeters();

    // control state
    void setTrackMuted(int channel, bool muted);
    void setTrackSoloed(int channel, bool soloed);
    void clearAllSoloed();
    bool hasSoloed() const;
    void setTracksInteractive(bool enabled);

    // layout
    void toggleTrackExpansion(int display_row);

    // event display
    void setEventViewMask(TrackEventViewMask mask);
    TrackEventViewMask eventViewMask() const { return m_event_view_mask; }
    void setEventPreset(TrackEventPreset preset);
    void setEventPreset(int preset_index);

    void setInstrumentContext(const VoiceGroup *song_voicegroup,
                              const QMap<QString, VoiceGroup> *all_voicegroups,
                              const QMap<QString, KeysplitTable> *keysplit_tables);

signals:
    void trackMuteToggled(int channel, bool muted);
    void trackSoloToggled(int channel, bool soloed);
    void trackSelected(int track);
    void layoutChanged();

private slots:
    void onTrackClicked(int track);

private:
    // drawing helpers
    void reset();
    void drawTracks();
    void relayout();

private:
    std::shared_ptr<Song> m_active_song;

    // scenes
    QGraphicsScene m_scene_roll;
    QGraphicsScene m_scene_track_list;

    // managed items
    QMap<int, GraphicsTrackItem *> m_track_items;
    QMap<int, GraphicsTrackRollManager *> m_roll_managers;

    // layout and event state
    int m_expanded_row = -1;
    TrackEventViewMask m_event_view_mask = TrackEventView_All;

    // instrument info
    const VoiceGroup *m_song_voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *m_all_voicegroups = nullptr;
    const QMap<QString, KeysplitTable> *m_keysplit_tables = nullptr;
};


#endif // TRACKROLL_H
