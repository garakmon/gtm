#pragma once
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>



namespace Ui { class MainWindow; }
class PianoRoll;
class TrackRoll;
class Song;

/*
 *  Controls playing of music from Player, scrolling of rolls, interaction between them
 */
class Controller : public QObject {
    Q_OBJECT

public:
    //
    Controller(QObject *parent = nullptr);

    bool loadSong(std::shared_ptr<Song> song); // TODO: should project::loadSong return shared_ptr<Song>?
    void display(Ui::MainWindow *window); // put onto views

private:
    // m_song, items lists?, send item edits to midifile edits
    std::shared_ptr<Song> m_song;
    PianoRoll *m_piano_roll = nullptr;
    TrackRoll *m_track_roll = nullptr;
};

#endif // CONTROLLER_H
