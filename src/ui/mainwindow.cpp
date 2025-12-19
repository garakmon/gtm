#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsEllipseItem>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>
#include <QDir>
#include <QGridLayout>

#include "constants.h"
#include "colors.h"
#include "theme.h"
#include "minimapwidget.h"
#include "pianoroll.h"
#include "trackroll.h"
#include "graphicstrackitem.h"
#include "previewsoundwindow.h"



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // TEMP: silence bug in my qt version re: mac trackpads [fix: update Qt version]
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));

    //this->m_project = std::make_unique<Project>();

    // TODO: temp
    loadProject();
    //loadSong();
}

MainWindow::~MainWindow() {
    // delete m_piano_roll;
    // delete m_track_roll;
    delete ui;
}

void MainWindow::setupUi() {
    // alloc new scenes
    // m_piano_roll = new PianoRoll(this);
    // this->ui->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // this->ui->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // this->ui->view_piano->setScene(m_piano_roll->scenePiano());
    // this->ui->view_piano->setSceneRect(m_piano_roll->scenePiano()->itemsBoundingRect());
    // this->ui->view_piano->setRenderHint(QPainter::Antialiasing, false);

    // this->ui->view_pianoRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // this->ui->view_pianoRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // this->ui->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    // this->ui->view_pianoRoll->setSceneRect(m_piano_roll->sceneRoll()->itemsBoundingRect());

    // this->ui->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    // this->ui->view_pianoRoll->centerOn(m_piano_roll->lines()[g_midi_middle_c]);

    // this->ui->view_piano->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));
    // this->ui->view_pianoRoll->setBackgroundBrush(QBrush(ui_color_piano_roll_bg, Qt::SolidPattern));

    // this->ui->view_piano->setVerticalScrollBar(this->ui->vscroll_pianoRoll);
    // this->ui->view_pianoRoll->setVerticalScrollBar(this->ui->vscroll_pianoRoll);

    // // track scene
    // m_track_roll = new TrackRoll(this);
    // this->ui->view_tracklist->setScene(this->m_track_roll->sceneRoll());
    // this->ui->view_tracklist->setSceneRect(this->m_track_roll->sceneRoll()->itemsBoundingRect());
    // this->ui->view_tracklist->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_controller = std::make_unique<Controller>(this);

    if (ui->widget_minimap) {
        m_minimap = new MinimapWidget(this);
        m_minimap->setObjectName("minimap");
        if (auto *grid = qobject_cast<QGridLayout *>(ui->scrollAreaWidgetContents_2->layout())) {
            grid->replaceWidget(ui->widget_minimap, m_minimap);
        }
        ui->widget_minimap->deleteLater();
        if (m_controller) {
            m_controller->setMinimap(m_minimap);
        }
    }

    // drawScoreArea();
}

void MainWindow::loadProject() {
    bool cfg_ok = false;
    m_config = GtmConfig::loadFromFile(GtmConfig::defaultPath(), &cfg_ok);
    qDebug() << "Config path:" << GtmConfig::defaultPath();
    ThemePalette palette = paletteByName(m_config.palette);
    applyTheme(palette, m_config.theme);
    applyThemeColors(palette);
    if (!cfg_ok || m_config.most_recent_project.isEmpty()) {
        return;
    }

    const QString root = m_config.most_recent_project;
    if (this->m_controller->loadProject(root)) {
        m_project_root = root;
    }

    //this->ui->listView_songTable->addItems()
}

void MainWindow::loadSong() {
    // load song from song list 
    // if (this->m_controller->loadSong(this->m_project->activeSong())) {
    //     this->m_controller->display();
    // }
    // else {
    //     // Failed to load a song for whatever reason
    // }
    // if (this->m_controller) {
    //     if (this->m_controller->loadSong()) {
    //         // TODO: change this?
    //         this->m_controller->displayRolls();
    //     }
    // }
}

void MainWindow::drawTrackList() {
    // 
}

void MainWindow::open(QString path) {
    qDebug() << "OPEN";
}

void MainWindow::on_action_Open_triggered() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"), ".", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!path.isEmpty()) {
        if (this->m_controller->loadProject(path)) {
            m_project_root = path;
            m_config.most_recent_project = path;
        }
    }
}

void MainWindow::on_action_PreviewSound_triggered() {
    //
    if (m_preview_sound_window) {
        m_preview_sound_window->show();
    }
    else {
        m_preview_sound_window = new PreviewSoundWindow(this);
        m_preview_sound_window->setAttribute(Qt::WA_DeleteOnClose);
        m_preview_sound_window->show();
    }
}

void MainWindow::on_Button_Play_clicked() {
    this->m_controller->play();
}

void MainWindow::on_Button_Pause_clicked() {
    this->m_controller->pause();
}

void MainWindow::on_Button_Stop_clicked() {
    this->m_controller->stop();
    this->m_controller->seekToStart();
}

void MainWindow::on_Button_Previous_clicked() {
    if (this->m_controller->currentTick() > 0) {
        this->m_controller->seekToStart();
        return;
    }
    auto *view = this->ui->listView_songTable;
    if (!view || !view->model()) return;
    QModelIndex current = view->currentIndex();
    int row = current.isValid() ? current.row() : 0;
    this->m_controller->selectSongByIndex(row - 1);
}

void MainWindow::on_Button_Skip_clicked() {
    auto *view = this->ui->listView_songTable;
    if (!view || !view->model()) return;
    QModelIndex current = view->currentIndex();
    int row = current.isValid() ? current.row() : 0;
    this->m_controller->selectSongByIndex(row + 1);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_project_root.isEmpty()) {
        m_config.most_recent_project = m_project_root;
        if (m_config.palette.isEmpty()) m_config.palette = "default";
        if (m_config.theme.isEmpty()) m_config.theme = "default";
        m_config.saveToFile(GtmConfig::defaultPath());
    }
    QMainWindow::closeEvent(event);
}
