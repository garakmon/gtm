#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsEllipseItem>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QFontMetrics>
#include <QSplitter>
#include <QFontDatabase>

#include "constants.h"
#include "colors.h"
#include "theme.h"
#include "minimapwidget.h"
#include "meters.h"
#include "pianoroll.h"
#include "trackroll.h"
#include "graphicstrackitem.h"
#include "previewsoundwindow.h"
#include <limits>
#include "customwidgets.h"



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // TEMP: silence bug in my qt version re: mac trackpads [fix: update Qt version]
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));

    QFontDatabase::addApplicationFont(":/fonts/IBMPlexMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/IBMPlexSans-Regular.ttf");

    //this->m_project = std::make_unique<Project>();

    // Restore window geometry early (if available)
    bool cfg_ok = false;
    m_config = GtmConfig::loadFromFile(GtmConfig::defaultPath(), &cfg_ok);
    if (cfg_ok && !m_config.window_geometry.isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(m_config.window_geometry.toUtf8()));
    }

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

    // Ensure Song/Track/Event groupboxes share vertical space evenly.
    if (ui->verticalLayout_3) {
        ui->verticalLayout_3->setStretch(0, 1);
        ui->verticalLayout_3->setStretch(1, 1);
        ui->verticalLayout_3->setStretch(2, 1);
    }

    if (ui->spinBox_NoteKey) {
        ui->spinBox_NoteKey->setRange(0, 127);
    }
    if (ui->spinBox_NoteChannel) {
        ui->spinBox_NoteChannel->setRange(0, 15);
    }
    if (ui->spinBox_NoteOnVelocity) {
        ui->spinBox_NoteOnVelocity->setRange(0, 127);
    }
    if (ui->spinBox_NoteOffVelocity) {
        ui->spinBox_NoteOffVelocity->setRange(0, 127);
    }
    if (ui->spinBox_NoteOnTick) {
        ui->spinBox_NoteOnTick->setRange(0, std::numeric_limits<int>::max());
    }
    if (ui->spinBox_NoteOffTick) {
        ui->spinBox_NoteOffTick->setRange(0, std::numeric_limits<int>::max());
    }

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

    if (ui->widget_masterMeter) {
        m_master_meter = new MasterMeterWidget(this);
        m_master_meter->setObjectName("masterMeter");
        if (auto *layout = qobject_cast<QHBoxLayout *>(ui->widget_masterMeter->parentWidget()->layout())) {
            layout->setAlignment(Qt::AlignVCenter);
            layout->replaceWidget(ui->widget_masterMeter, m_master_meter);
        }
        ui->widget_masterMeter->deleteLater();
        if (m_controller) {
            m_controller->setMasterMeter(m_master_meter);
            connect(m_master_meter->slider(), &QSlider::valueChanged, m_controller.get(), &Controller::setMasterVolume);
            m_controller->setMasterVolume(m_master_meter->slider()->value());
        }
    }

    if (auto *icon = qobject_cast<GTMSvgIconWidget *>(ui->label_volume)) {
        icon->setFixedSize(16, 16);
        icon->setUseTextColor(false);

        auto updateIcon = [icon](int value) {
            const char *path = ":/icons/sound-high.svg";
            if (value <= 0) {
                path = ":/icons/sound-off.svg";
            } else if (value <= 20) {
                path = ":/icons/sound-min.svg";
            } else if (value <= 60) {
                path = ":/icons/sound-low.svg";
            }
            icon->setSvgPath(path);
        };

        if (m_master_meter && m_master_meter->slider()) {
            connect(m_master_meter->slider(), &QSlider::valueChanged, icon, updateIcon);
            updateIcon(m_master_meter->slider()->value());
        } else {
            updateIcon(0);
        }
    }

    if (auto *search = qobject_cast<GTMLineEdit *>(ui->lineEdit_SongList_search)) {
        search->setLeadingSvg(":/icons/search-engine.svg", 14);
    }

    if (ui->Button_Play) ui->Button_Play->setCheckable(true);
    if (ui->Button_Pause) ui->Button_Pause->setCheckable(true);
    if (ui->Button_Stop) ui->Button_Stop->setCheckable(true);

    auto setFixedLabelWidth = [](QLabel *label, const QString &sample) {
        if (!label) return;
        QFontMetrics fm(label->font());
        label->setFixedWidth(fm.horizontalAdvance(sample));
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    };
    setFixedLabelWidth(ui->label_MetaTickValue, "1234567");
    if (ui->label_MetaTimeValue) {
        ui->label_MetaTimeValue->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->label_MetaTimeValue->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        QFontMetrics fm(ui->label_MetaTimeValue->font());
        ui->label_MetaTimeValue->setMinimumWidth(fm.horizontalAdvance("00:00.000") + 8);
    }
    setFixedLabelWidth(ui->label_MetaMeasureBeatValue, "123.16");
    setFixedLabelWidth(ui->label_MetaTempoValue, "240.0 BPM");
    setFixedLabelWidth(ui->label_MetaTimeSigValue, "12/16");

    auto updateSplitterVisibility = [this]() {
        if (!ui->splitter || !ui->scrollArea || !ui->scroll_pianoArea) return;
        if (ui->scrollArea->isVisible() && ui->scroll_pianoArea->isVisible()) {
            m_splitter_sizes = ui->splitter->sizes();
        }
        const bool show_all = ui->radioButton_pianoAll && ui->radioButton_pianoAll->isChecked();
        const bool show_events = show_all || (ui->radioButton_pianoEvents && ui->radioButton_pianoEvents->isChecked());
        const bool show_notes = show_all || (ui->radioButton_pianoNotes && ui->radioButton_pianoNotes->isChecked());

        ui->scrollArea->setVisible(show_events);
        ui->scroll_pianoArea->setVisible(show_notes);

        int total = ui->splitter->height();
        if (total <= 0) total = 1;
        if (show_events && show_notes) {
            if (m_splitter_sizes.size() == 2 && (m_splitter_sizes[0] + m_splitter_sizes[1]) > 0) {
                ui->splitter->setSizes(m_splitter_sizes);
            } else {
                ui->splitter->setSizes({total / 2, total - (total / 2)});
            }
        } else if (show_events) {
            ui->splitter->setSizes({total, 0});
        } else if (show_notes) {
            ui->splitter->setSizes({0, total});
        }
    };

    if (ui->radioButton_pianoAll) {
        connect(ui->radioButton_pianoAll, &QRadioButton::toggled, this, updateSplitterVisibility);
    }
    if (ui->radioButton_pianoEvents) {
        connect(ui->radioButton_pianoEvents, &QRadioButton::toggled, this, updateSplitterVisibility);
    }
    if (ui->radioButton_pianoNotes) {
        connect(ui->radioButton_pianoNotes, &QRadioButton::toggled, this, updateSplitterVisibility);
    }
    if (ui->splitter) {
        connect(ui->splitter, &QSplitter::splitterMoved, this, [this](int, int) {
            if (!ui->scrollArea || !ui->scroll_pianoArea) return;
            if (ui->scrollArea->isVisible() && ui->scroll_pianoArea->isVisible()) {
                m_splitter_sizes = ui->splitter->sizes();
            }
        });
    }
    updateSplitterVisibility();

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
    if (ui->Button_Play) ui->Button_Play->setChecked(true);
    if (ui->Button_Pause) ui->Button_Pause->setChecked(false);
    if (ui->Button_Stop) ui->Button_Stop->setChecked(false);
    this->m_controller->play();
}

void MainWindow::on_Button_Pause_clicked() {
    if (ui->Button_Play) ui->Button_Play->setChecked(false);
    if (ui->Button_Pause) ui->Button_Pause->setChecked(true);
    if (ui->Button_Stop) ui->Button_Stop->setChecked(false);
    this->m_controller->pause();
}

void MainWindow::on_Button_Stop_clicked() {
    if (ui->Button_Play) ui->Button_Play->setChecked(false);
    if (ui->Button_Pause) ui->Button_Pause->setChecked(false);
    if (ui->Button_Stop) ui->Button_Stop->setChecked(true);
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
    auto *model = view->model();
    int rows = model->rowCount();
    if (rows <= 0) return;
    int row = this->m_controller ? this->m_controller->currentSongIndex() : -1;
    if (row < 0) {
        QModelIndex current = view->currentIndex();
        row = current.isValid() ? current.row() : 0;
    }
    for (int next = row - 1; next >= 0; --next) {
        QModelIndex idx = model->index(next, 0);
        if (idx.isValid() && (model->flags(idx) & Qt::ItemIsEnabled)) {
            this->m_controller->selectSongByIndex(next);
            return;
        }
    }
}

void MainWindow::on_Button_Skip_clicked() {
    auto *view = this->ui->listView_songTable;
    if (!view || !view->model()) return;
    auto *model = view->model();
    int rows = model->rowCount();
    if (rows <= 0) return;
    int row = this->m_controller ? this->m_controller->currentSongIndex() : -1;
    if (row < 0) {
        QModelIndex current = view->currentIndex();
        row = current.isValid() ? current.row() : 0;
    }
    for (int next = row + 1; next < rows; ++next) {
        QModelIndex idx = model->index(next, 0);
        if (idx.isValid() && (model->flags(idx) & Qt::ItemIsEnabled)) {
            this->m_controller->selectSongByIndex(next);
            return;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_project_root.isEmpty()) {
        m_config.most_recent_project = m_project_root;
        if (m_config.palette.isEmpty()) m_config.palette = "default";
        if (m_config.theme.isEmpty()) m_config.theme = "default";
        m_config.window_geometry = QString::fromUtf8(saveGeometry().toBase64());
        m_config.saveToFile(GtmConfig::defaultPath());
    }
    QMainWindow::closeEvent(event);
}
