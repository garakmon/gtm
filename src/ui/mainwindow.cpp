#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsEllipseItem>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>

#include "constants.h"
#include "colors.h"
#include "pianoroll.h"
#include "graphicstrackitem.h"



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // TEMP: silence bug in my qt version re: mac trackpads [fix: update Qt version]
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));

    this->m_project = std::make_unique<Project>();

    loadProject();
    loadSong();
}

MainWindow::~MainWindow() {
    // delete m_piano_roll;
    delete ui;
}

void MainWindow::setupUi() {
    // Piano scroll tabs
    // this->ui->tab_showMidiEvents->addTab("All");
    // this->ui->tab_showMidiEvents->addTab("Notes");
    // this->ui->tab_showMidiEvents->addTab("Events");
    // this->ui->tab_showMidiEvents->show();

    // alloc new scenes
    m_piano_roll = new PianoRoll(this);
    this->ui->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_piano->setScene(m_piano_roll->scenePiano());
    this->ui->view_piano->setSceneRect(m_piano_roll->scenePiano()->itemsBoundingRect());
    this->ui->view_piano->setRenderHint(QPainter::Antialiasing, false);

    this->ui->view_pianoRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_pianoRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    this->ui->view_pianoRoll->setSceneRect(m_piano_roll->sceneRoll()->itemsBoundingRect());

    this->ui->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    this->ui->view_pianoRoll->centerOn(m_piano_roll->lines()[g_midi_middle_c]);

    this->ui->view_piano->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));
    this->ui->view_pianoRoll->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));

    this->ui->view_piano->setVerticalScrollBar(this->ui->vscroll_pianoRoll);
    this->ui->view_pianoRoll->setVerticalScrollBar(this->ui->vscroll_pianoRoll);

    // track scene
    m_scene_tracks = new QGraphicsScene(this);
    this->ui->view_tracklist->setScene(this->m_scene_tracks);

    // drawScoreArea();
}

void MainWindow::loadProject() {
    m_project->load();

    //this->ui->listView_songTable->addItems()
}

void MainWindow::loadSong() {
    // load song from song list 
    drawScoreArea();
    drawTrackList();
}

void MainWindow::drawScoreArea() {
    // TODO: move piano roll stuff to here
    this->m_piano_roll->setSong(this->m_project->activeSong());
    // populate graphicsscorenoteitems
    // pianoroll scene score clear
    // score area only redrawn when changes, otherwise translating the area based on measure, (virtual pixels? or scroll down too?)
}

void MainWindow::drawTrackList() {
    // 
    this->m_scene_tracks->clear();

    std::shared_ptr<Song> song = this->m_project->activeSong();

    int track_num = 0;
    for (auto track : song->tracks()) {
        this->m_scene_tracks->addItem(new GraphicsTrackItem(track_num));
        track_num++;
    }

    this->ui->view_tracklist->setSceneRect(this->m_scene_tracks->itemsBoundingRect());
    this->ui->view_tracklist->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

void MainWindow::open(QString path) {
    qDebug() << "OPEN";
}

QString MainWindow::browse(QString filter, QFileDialog::FileMode mode) {
    QFileDialog browser;
    browser.setFileMode(mode);
    QString filepath = browser.getOpenFileName(this, "Select a File", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), filter);
    return filepath;
}

// void MainWindow::on_action_open_triggered() {
//     QString path = browse("Midi Files (*.mid *.midi *.MID *.MIDI)", QFileDialog::ExistingFile);
//     open(path);
// }


