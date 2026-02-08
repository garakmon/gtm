#pragma once
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdint>

#include "project.h"
#include "projectinterface.h"
#include "player.h"



namespace Ui { class MainWindow; }
class MainWindow;
class PianoRoll;
class TrackRoll;
class MeasureRoll;
class Song;
class SongListModel;
class MinimapWidget;
class MasterMeterWidget;
namespace smf { class MidiEvent; }

/*
 *  Controls playing of music from Player, scrolling of rolls, interaction between them
 */
class Controller : public QObject {
    Q_OBJECT

public:
    //
    Controller(MainWindow *window = nullptr);

    bool loadProject(const QString &root);
    bool loadSong();
    bool loadSong(std::shared_ptr<Song> song); // TODO: should project::loadSong return shared_ptr<Song>?
    void displayRolls(); // put onto views
    void displayProject();

    void setupRolls(); // one time actions

    void syncRolls();
    void connectSignals();

    void readTracks();

    void play();
    void stop();
    void pause();
    void seekToTick(int tick);
    void seekToStart();
    int currentTick() const;
    int currentSongIndex() const;
    bool selectSongByIndex(int index);
    bool selectSongByTitle(const QString &title);
    void setMinimap(MinimapWidget *minimap);
    void setMasterMeter(MasterMeterWidget *meter);
    void setMasterVolume(int value);
    void setTrackEventViewMask(uint32_t mask);
    void setTrackEventPreset(int preset_index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void songSelected(const QString &title);

private slots:
    void displayEvent(smf::MidiEvent *event);
    void songListSongRequested(const QModelIndex &index);
    void onTrackMuteToggled(int channel, bool muted);
    void onTrackSoloToggled(int channel, bool soloed);
    void onTrackSelected(int track);

private:
    void updateSongMetaDisplay();
    void updateSongPositionDisplay(int tick);

    std::unique_ptr<Project> m_project;
    std::unique_ptr<ProjectInterface> m_interface;

    // m_song, items lists?, send item edits to midifile edits
    Ui::MainWindow *m_window = nullptr;
    std::shared_ptr<Song> m_song;
    QPointer<PianoRoll> m_piano_roll;
    QPointer<TrackRoll> m_track_roll;
    QPointer<MeasureRoll> m_measure_roll;
    QPointer<MinimapWidget> m_minimap;
    QPointer<MasterMeterWidget> m_master_meter;
    QPointer<SongListModel> m_song_list_model;

    // playback stuff
    QTimer m_player_timer;
    QElapsedTimer m_player_elapsed;
    int m_playback_start_tick = 0;
    int m_meter_frame = 0;
    int m_scroll_frame = 0;
    int m_last_meta_tick = -1;
    int m_last_play_tick = -1;
    bool m_force_scroll_to_playhead = false;
    bool m_edits_enabled = true;
    double m_scroll_pos = 0.0;
    bool m_scroll_pos_valid = false;
    bool m_autoscroll_prev = true;
    int m_current_song_index = -1;
    int m_song_duration_ticks = 0;

    // scroll state
    QTimer m_scroll_debounce;
    bool m_autoscroll_enabled = true;

    std::unique_ptr<Player> m_player;
};

#endif // CONTROLLER_H
