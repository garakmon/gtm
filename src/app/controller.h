#pragma once
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QPointer>



namespace Ui { class MainWindow; }
class PianoRoll;
class TrackRoll;
class MeasureRoll;
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

    void readTracks();

private:
    // m_song, items lists?, send item edits to midifile edits
    std::shared_ptr<Song> m_song;
    QPointer<PianoRoll> m_piano_roll;
    QPointer<TrackRoll> m_track_roll;
    QPointer<MeasureRoll> m_measure_roll;
};

#endif // CONTROLLER_H
