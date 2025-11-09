#pragma once
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>

#include "project.h"
#include "projectinterface.h"



namespace Ui { class MainWindow; }
class MainWindow;
class PianoRoll;
class TrackRoll;
class MeasureRoll;
class Song;
class SongListModel;
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

    void syncRolls();
    void connectSignals();

    void readTracks();

    void play();
    void stop();

private slots:
    void displayEvent(smf::MidiEvent *event);
    void songListSongRequested(const QModelIndex &index);

private:
    std::unique_ptr<Project> m_project;
    std::unique_ptr<ProjectInterface> m_interface;

    // m_song, items lists?, send item edits to midifile edits
    Ui::MainWindow *m_window = nullptr;
    std::shared_ptr<Song> m_song;
    QPointer<PianoRoll> m_piano_roll;
    QPointer<TrackRoll> m_track_roll;
    QPointer<MeasureRoll> m_measure_roll;
    QPointer<SongListModel> m_song_list_model;

    // playback stuff
    QTimer m_player_timer;
    QElapsedTimer m_player_elapsed;
};

#endif // CONTROLLER_H
