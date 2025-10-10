#include "controller.h"




#include "../ui/ui_mainwindow.h"
#include "mainwindow.h"
#include "pianoroll.h"
#include "trackroll.h"
#include "measureroll.h"
#include "constants.h"
#include "colors.h"
#include "song.h"
#include "MidiEvent.h"


/// controller loads the song, creates the track items, etc.
Controller::Controller(MainWindow *window) : QObject(window) {
    m_window = window->ui;
    m_piano_roll = new PianoRoll(this);
    connect(m_piano_roll, &PianoRoll::eventItemSelected, this, &Controller::displayEvent);
    m_track_roll = new TrackRoll(this);
    m_measure_roll = new MeasureRoll(this);
}

void Controller::display() {
    this->m_piano_roll->display();
    //
    m_window->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_piano->setScene(m_piano_roll->scenePiano());
    m_window->view_piano->setSceneRect(m_piano_roll->scenePiano()->itemsBoundingRect());
    m_window->view_piano->setRenderHint(QPainter::Antialiasing, false);

    m_window->view_pianoRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_pianoRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    m_window->view_pianoRoll->setSceneRect(m_piano_roll->sceneRoll()->itemsBoundingRect());

    m_window->view_piano->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));
    m_window->view_pianoRoll->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));

    // connect vertical scrolling piano
    m_window->view_piano->setVerticalScrollBar(m_window->vscroll_pianoRoll);
    m_window->view_pianoRoll->setVerticalScrollBar(m_window->vscroll_pianoRoll);

    // connect vertical scrolling tracks

    // track scene
    m_window->view_trackRoll->setScene(m_track_roll->sceneRoll());
    m_window->view_trackRoll->setSceneRect(m_track_roll->sceneRoll()->itemsBoundingRect());
    m_window->view_trackRoll->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_window->view_trackList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_trackList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_trackList->setScene(m_track_roll->sceneTracks());
    m_window->view_trackList->setSceneRect(m_track_roll->sceneTracks()->itemsBoundingRect());

    // measures
    m_window->view_measures_tracks->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures_tracks->setSceneRect(m_measure_roll->sceneMeasures()->itemsBoundingRect());
    m_window->view_measures_tracks->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_measures_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_window->view_measures->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures->setSceneRect(m_measure_roll->sceneMeasures()->itemsBoundingRect());
    m_window->view_measures->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_measures->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // connect horizontal scrolling
    m_window->view_pianoRoll->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures_tracks->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    //m_window->view_tracklist->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    //m_window->hscroll_pianoRoll->setValue(0);

    //m_window->view_pianoRoll->centerOn();
    m_window->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    //m_window->hscroll_pianoRoll->setValue(m_window->hscroll_pianoRoll->minimum());
    //m_window->view_pianoRoll->centerOn(m_piano_roll->lines()[g_midi_middle_c]);
}

bool Controller::loadSong(std::shared_ptr<Song> song) {
    //
    this->m_track_roll->setSong(song);
    this->m_piano_roll->setSong(song);
    this->m_measure_roll->setSong(song);

    song->linkNotePairs();

    double duration;
    int track_num = 0;
    for (auto track : song->tracks()) {
        qDebug() << "\n*** TRACK" << track_num << "***\n";
        bool meta_track = true;
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *midi_event = &(*track)[i];
            if (midi_event->isNote()) {
                if (midi_event->isNoteOn()) this->m_piano_roll->addNote(track_num, midi_event);
                meta_track = false;
                continue;
            }
            else if (midi_event->isMetaMessage()) {
                qDebug() << "MetaMessage: type" << midi_event->getMetaType();
            }
            else if (midi_event->isController()) {
                meta_track = false;
                qDebug() << "Controller:" << midi_event->getControllerNumber() << midi_event->getControllerValue();
            }
            else if (midi_event->isPatchChange()) {
                meta_track = false;
                qDebug() << "Patch Change:" << midi_event->getChannel() << midi_event->getP1();
            }
            else if (midi_event->isPressure()) {
                meta_track = false;
                qDebug() << "Pressure:" << midi_event->getP1();
            }
            else if (midi_event->isPitchbend()) {
                meta_track = false;
                qDebug() << "Pitch Bend:" << midi_event->getP1() << midi_event->getP2();
            }
            else {
                qDebug() << "unknown midi message type";
            }
        }
        track_num++;
    }
    qDebug() << "load sucess";
    return true;
}

void Controller::displayEvent(smf::MidiEvent *event) {
    //
    qDebug() << "Controller::displayEvent";
}
