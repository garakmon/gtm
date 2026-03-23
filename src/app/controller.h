#pragma once
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "app/project.h"
#include "app/projectinterface.h"
#include "sound/player.h"

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <cstdint>



namespace Ui { class MainWindow; }
class MainWindow;
class PianoRoll;
class TrackRoll;
class MeasureRoll;
class Song;
class SongListModel;
class SongEditor;
class MinimapWidget;
class MasterMeterWidget;
namespace smf { class MidiEvent; }

//////////////////////////////////////////////////////////////////////////////////////////
///
/// The Controller is the main class responsible for coordinating the app, ui, audio
/// engine, and project. It owns the Project (as m_project) and Player (as m_player).
/// Additionally, the Controller manages the playback state.
///
//////////////////////////////////////////////////////////////////////////////////////////
class Controller : public QObject {
    Q_OBJECT

public:
    Controller(MainWindow *window = nullptr); // constructor

    // disable copy and move ops
    Controller(const Controller &) = delete;
    Controller &operator=(const Controller &) = delete;
    Controller(Controller &&) = delete;
    Controller &operator=(Controller &&) = delete;

    // project + interface / loading + saving
    bool loadProject(const QString &root);
    const QString &projectRoot() const;
    bool createSong(const NewSongSettings &settings, QString *error = nullptr);
    bool hasUnsavedChanges() const;
    QStringList voicegroupNames() const;
    const QMap<QString, VoiceGroup> &voicegroups() const;
    const QMap<QString, KeysplitTable> &keysplitTables() const;
    QStringList playerNames() const;
    QStringList songTitles() const;
    SongEditor *songEditor() const;
    bool loadSong();
    bool loadSong(std::shared_ptr<Song> song); // !TODO: return shared_ptr<Song>?
    bool canUndoHistory() const;
    bool canRedoHistory() const;
    void undoHistory();
    void redoHistory();

    // ui coordination
    void setupRolls();     // one time actions
    void displayRolls();   // put onto views
    void displayProject(); // set up project-level ui
    void connectSignals(); // once per project signal initialization

    void syncRolls();                           // update ui state to match playback state
    void setMinimap(MinimapWidget *minimap);    // attach minimap from MainWindow
    void setTrackEventViewMask(uint32_t mask);  // sets track-meta event type visiblity
    void setTrackEventPreset(int preset_index); // applies a preset view mask

    int currentSongIndex() const;                 // get the index of the current song
    bool selectSongByIndex(int index);            // load a song by its index in song list
    bool selectSongByTitle(const QString &title); // load a song by its title

    // song playback
    void play();  // start playback from current playhead position
    void stop();  // end playback and set playhead to start of song
    void pause(); // end playback but keep playhead position

    int currentTick() const;         // get the tick position
    void seekToTick(int tick);       // jump to a specific tick
    void seekToStart();              // jump to tick 0
    void setMasterVolume(int value); // set a master volume for the Mixer

    // song visualization
    void setMasterMeter(MasterMeterWidget *meter); // attach ui master meter

private:
    void updateSongMetaDisplay();
    void updateSongPositionDisplay(int tick);
    void rebuildInferredKeySignatureCache();
    QPair<int, bool> keySignatureForTick(int tick) const;
    void redrawCurrentSong();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override; // user scroll override

signals:
    void songSelected(const QString &title);

private slots:
    void displayEvent(smf::MidiEvent *event);
    void onSelectedEventsChanged(const QVector<smf::MidiEvent *> &events);
    void onNoteMoveRequested(const NoteMoveSettings &settings);
    void onNoteResizeRequested(const NoteResizeSettings &settings);
    void onSongNeedsRedrawing();
    void songListSongRequested(const QModelIndex &index);
    void onTrackMuteToggled(int channel, bool muted);
    void onTrackSoloToggled(int channel, bool soloed);
    void onTrackSelected(int track);
    void onTrackLayoutChanged();

private:
    // owned objects
    std::unique_ptr<Project> m_project;
    std::unique_ptr<ProjectInterface> m_interface;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<SongEditor> m_song_editor;

    // non-owning references
    Ui::MainWindow *m_window = nullptr;
    std::shared_ptr<Song> m_song;

    // ui objects
    QPointer<PianoRoll> m_piano_roll;
    QPointer<TrackRoll> m_track_roll;
    QPointer<MeasureRoll> m_measure_roll;
    QPointer<MinimapWidget> m_minimap;
    QPointer<MasterMeterWidget> m_master_meter;
    QPointer<SongListModel> m_song_list_model;

    // playback stuff
    QTimer m_player_timer;
    QElapsedTimer m_player_elapsed;
    int m_meter_frame = 0;

    struct PlaybackState {
        int playback_start_tick = 0;
        int last_play_tick = -1;
        int last_meta_tick = -1;
        int song_duration_ticks = 0;
        int current_song_index = -1;
        bool force_scroll_to_playhead = false;
        bool edits_enabled = true;
    } m_playback_state;

    struct ScrollState {
        bool autoscroll_enabled = true;
        double scroll_pos = 0.0;
        bool scroll_pos_valid = false;
        bool autoscroll_prev = true;
        int scroll_frame = 0;
        QTimer scroll_debounce;
    } m_scroll_state;

    QMap<int, QPair<int, bool>> m_inferred_key_by_tick;
};

#endif // CONTROLLER_H
