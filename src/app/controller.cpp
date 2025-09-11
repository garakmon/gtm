#include "controller.h"




#include "../ui/ui_mainwindow.h"
#include "pianoroll.h"
#include "trackroll.h"
#include "constants.h"
#include "colors.h"
#include "song.h"


/// controller loads the song, creates the track items, etc.
Controller::Controller(QObject *parent) : QObject(parent) {
    m_piano_roll = new PianoRoll(this);
    m_track_roll = new TrackRoll(this);
}

void Controller::display(Ui::MainWindow *window) {
    //
    window->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    window->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    window->view_piano->setScene(m_piano_roll->scenePiano());
    window->view_piano->setSceneRect(m_piano_roll->scenePiano()->itemsBoundingRect());
    window->view_piano->setRenderHint(QPainter::Antialiasing, false);

    window->view_pianoRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    window->view_pianoRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    window->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    window->view_pianoRoll->setSceneRect(m_piano_roll->sceneRoll()->itemsBoundingRect());

    window->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    window->view_pianoRoll->centerOn(m_piano_roll->lines()[g_midi_middle_c]);

    window->view_piano->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));
    window->view_pianoRoll->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));

    window->view_piano->setVerticalScrollBar(window->vscroll_pianoRoll);
    window->view_pianoRoll->setVerticalScrollBar(window->vscroll_pianoRoll);

    // track scene
    window->view_tracklist->setScene(this->m_track_roll->sceneRoll());
    window->view_tracklist->setSceneRect(this->m_track_roll->sceneRoll()->itemsBoundingRect());
    window->view_tracklist->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

bool Controller::loadSong(std::shared_ptr<Song> song) {
    //
    this->m_track_roll->setSong(song);
    this->m_piano_roll->setSong(song);

    song->linkNotePairs();

    double duration;
    int track_num = 0;
    for (auto track : song->tracks()) {
        // add track
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *midi_event = &(*track)[i];
            if (!midi_event->isNoteOn()) {
                continue;
            }
            else {
                //qDebug() << "event duration:" << midi_event->getTickDuration();
                this->m_piano_roll->addNote(track_num, midi_event);
            }
            //duration = mev->getTickDuration();            
        }
        track_num++;
    }

    return true;
}
