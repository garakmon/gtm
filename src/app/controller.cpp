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
#include "project.h"
#include "projectinterface.h"
#include "songlistmodel.h"


/// controller loads the song, creates the track items, etc.
Controller::Controller(MainWindow *window) : QObject(window) {
    m_window = window->ui;
    m_piano_roll = new PianoRoll(this);
    connect(m_piano_roll, &PianoRoll::eventItemSelected, this, &Controller::displayEvent);
    m_track_roll = new TrackRoll(this);
    m_measure_roll = new MeasureRoll(this);

    connect(&this->m_player_timer, &QTimer::timeout, this, &Controller::syncRolls);
}

void Controller::displayRolls() {
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
    m_window->view_trackList->setVerticalScrollBar(m_window->vscroll_trackRoll);
    m_window->view_trackRoll->setVerticalScrollBar(m_window->vscroll_trackRoll);

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
    //m_window->view_measures_tracks->setSceneRect(m_measure_roll->sceneMeasures()->itemsBoundingRect());
    m_window->view_measures_tracks->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_measures_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //m_window->view_measures->setFrameShape(QFrame::NoFrame);
    // Ensure the view is tall enough to hold the 30px scene + zero margins
    //m_window->view_measures->setFixedHeight(32);
    //m_window->view_measures->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_window->view_measures->setScene(m_measure_roll->sceneMeasures());
    //m_window->view_measures->setSceneRect(m_measure_roll->sceneMeasures()->itemsBoundingRect());
    m_window->view_measures->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_measures->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // connect horizontal scrolling
    m_window->view_pianoRoll->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures_tracks->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    
    m_window->view_trackRoll->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    //m_window->hscroll_pianoRoll->setValue(0);

    //m_window->view_pianoRoll->centerOn();
    m_window->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    //m_window->hscroll_pianoRoll->setValue(m_window->hscroll_pianoRoll->minimum());
    //m_window->view_pianoRoll->centerOn(m_piano_roll->lines()[g_midi_middle_c]);

    this->m_window->LCD_Timer->setDigitCount(9);
}

void Controller::displayProject() {
    // display the song table list
    this->m_song_list_model = new SongListModel(this->m_project.get(), this);
    this->m_window->listView_songTable->setModel(this->m_song_list_model);

    this->connectSignals();
}

void Controller::connectSignals() {
    connect(this->m_window->listView_songTable, &QListView::doubleClicked, this, &Controller::songListSongRequested, Qt::UniqueConnection);
}

void Controller::songListSongRequested(const QModelIndex &index) {
    QString title = index.data(Qt::DisplayRole).toString();

    SongEntry entry = this->m_project->getSongEntryByTitle(title);

    // std::shared_ptr<Song> song = this->m_project->getSong(title);
    // !TODO: load unloaded songs here (well, in the project function)

    qDebug() << "Clicked" << title;
    qDebug() << "voicegroup:" << entry.voicegroup << "player:" << entry.player << "file:" << entry.midifile;
}

void Controller::syncRolls() {
    if (!this->m_song) {
        qDebug() << "NOO";
        return;
    }
    double elapsed_seconds = this->m_player_elapsed.elapsed() / 1000.0;

    QTime display_time(0, 0);
    display_time = display_time.addMSecs(this->m_player_elapsed.elapsed());
    QString time_text = display_time.toString("mm:ss.zzz");
    this->m_window->LCD_Timer->display(time_text);

    int current_tick = this->m_song->getTickFromTime(elapsed_seconds);

    this->m_measure_roll->updatePlaybackGuide(current_tick);
    // this->m_piano_roll->updatePlaybackGuide(current_tick);
    // this->m_track_roll->updatePlaybackGuide(current_tick);

    int offset = this->m_window->view_measures->viewport()->width() / 4;
    int scroll_value = static_cast<int>(current_tick * ui_tick_x_scale - offset);
    this->m_window->hscroll_pianoRoll->setValue(scroll_value);

    if (current_tick >= this->m_song->durationInTicks()) {
        this->stop();
    }
}

bool Controller::loadProject(const QString &root) {
    if (!this->m_project) {
        this->m_project = std::make_unique<Project>();
        this->m_interface = std::make_unique<ProjectInterface>(this->m_project.get());
    }

    if (this->m_interface->loadProject(root)) {
        // display the project values in the ui
        this->displayProject();
    }
}

bool Controller::loadSong() {
    return this->loadSong(this->m_project->activeSong());
}

bool Controller::loadSong(std::shared_ptr<Song> song) {
    //

    song->load();

    this->m_song = song;

    this->m_track_roll->setSong(song);
    this->m_piano_roll->setSong(song);
    this->m_measure_roll->setSong(song);

    return true;
}

void Controller::displayEvent(smf::MidiEvent *event) {
    // TODO: safety checks (isNote / isNoteOn, etc)
    qDebug() << "Controller::displayEvent";
    if (!event) {
        // error
        return;
    }

    m_window->spinBox_NoteKey->setValue(event->getKeyNumber());
    m_window->spinBox_NoteOnTick->setValue(event->tick);
    m_window->spinBox_NoteOnVelocity->setValue(event->getVelocity());
    if (event->hasLink()) {
        m_window->spinBox_NoteOffTick->setValue(event->getLinkedEvent()->tick);
        m_window->spinBox_NoteOffVelocity->setValue(event->getLinkedEvent()->getVelocity());
    }
}

void Controller::play() {
    // TODO: take tick as arg to play from specific place?
    this->m_player_elapsed.start();
    this->m_player_timer.start(16); // ~60fps is smooth enough scrolling
}

void Controller::stop() {
    //
    this->m_player_timer.stop();
    //this->m_player_elapsed.stop();
}
