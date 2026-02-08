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
#include <QButtonGroup>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QToolButton>
#include <QSignalBlocker>
#include <QComboBox>
#include <QVBoxLayout>

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

namespace {
constexpr int kPresetAll = 0;
constexpr int kPresetMix = 1;
constexpr int kPresetTimbre = 2;
constexpr int kPresetOther = 3;
constexpr int kPresetCustom = 4;

TrackEventViewMask presetMaskForIndex(int idx) {
    switch (idx) {
    case kPresetAll:
        return kTrackEventView_All;
    case kPresetMix:
        return TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan;
    case kPresetTimbre:
        return TrackEventView_Program;
    case kPresetOther:
        return TrackEventView_ControlOther;
    default:
        return 0;
    }
}

int presetIndexForMask(TrackEventViewMask mask) {
    if (mask == kTrackEventView_All) return kPresetAll;
    if (mask == (TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan)) return kPresetMix;
    if (mask == TrackEventView_Program) return kPresetTimbre;
    if (mask == TrackEventView_ControlOther) return kPresetOther;
    return kPresetCustom;
}
} // namespace


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
    //! TODO: break into multiple functions
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
    connect(m_controller.get(), &Controller::songSelected, this, [this](const QString &title) {
        setRecentSongTitle(title);
        syncSongListSelectionToOpenSong(false);
    });
    setupTrackMetaControls();

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
        if (m_master_meter->slider()) {
            connect(m_master_meter->slider(), &QSlider::valueChanged, this, [this](int value) {
                m_config.master_volume = value;
            });
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
        connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (!m_song_filter) return;
            if (text.trimmed().isEmpty()) {
                m_song_filter->setFilterRegularExpression(QRegularExpression());
                syncSongListSelectionToOpenSong(true);
                return;
            }
            QRegularExpression re(QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption);
            m_song_filter->setFilterRegularExpression(re);
        });
    }

    if (ui->ButtonBox_Tools) {
        auto *tool_group = new QButtonGroup(this);
        tool_group->setExclusive(true);
        const auto tool_buttons = ui->ButtonBox_Tools->findChildren<QToolButton *>();
        for (auto *button : tool_buttons) {
            button->setCheckable(true);
            tool_group->addButton(button);
        }
        if (ui->button_select_rect) {
            ui->button_select_rect->setChecked(true);
        }
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

    if (ui->listView_songTable) {
        auto *model = ui->listView_songTable->model();
        if (model) {
            if (!m_song_filter) {
                m_song_filter = new QSortFilterProxyModel(this);
                m_song_filter->setFilterKeyColumn(0);
                m_song_filter->setFilterRole(Qt::UserRole);
                m_song_filter->setDynamicSortFilter(true);
            }
            m_song_filter->setSourceModel(model);
            ui->listView_songTable->setModel(m_song_filter);
        }
    }

    if (m_master_meter && m_master_meter->slider()) {
        int volume = qBound(0, m_config.master_volume, 100);
        m_master_meter->slider()->setValue(volume);
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

void MainWindow::setupTrackMetaControls() {
    if (!ui || !ui->frame_TrackMetaControls) return;

    if (auto *old_layout = ui->frame_TrackMetaControls->layout()) {
        QLayoutItem *child = nullptr;
        while ((child = old_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        delete old_layout;
    }

    auto *root_layout = new QVBoxLayout(ui->frame_TrackMetaControls);
    root_layout->setContentsMargins(1, 1, 1, 1);
    root_layout->setSpacing(1);

    m_track_event_preset_combo = new GTMComboBox(ui->frame_TrackMetaControls);
    m_track_event_preset_combo->addItem("All");
    m_track_event_preset_combo->addItem("Mix");
    m_track_event_preset_combo->addItem("Timbre");
    m_track_event_preset_combo->addItem("Other");
    m_track_event_preset_combo->addItem("Custom");
    m_track_event_preset_combo->setCurrentIndex(kPresetAll);
    m_track_event_preset_combo->setMinimumHeight(16);
    m_track_event_preset_combo->setMaximumHeight(16);
    root_layout->addWidget(m_track_event_preset_combo);

    auto *button_row_layout = new QGridLayout();
    button_row_layout->setContentsMargins(0, 0, 0, 0);
    button_row_layout->setHorizontalSpacing(1);
    button_row_layout->setVerticalSpacing(0);
    root_layout->addLayout(button_row_layout);

    auto addFilterButton = [this, button_row_layout](int col, const QString &text, TrackEventViewMask flag, const QString &tooltip) {
        auto *btn = new QToolButton(ui->frame_TrackMetaControls);
        btn->setText(text);
        btn->setCheckable(true);
        btn->setChecked((m_track_event_mask_ui & flag) != 0);
        btn->setToolTip(QString("%1\nRight-click: solo this event type").arg(tooltip));
        btn->setAutoRaise(false);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(15, 15);
        QFont f = btn->font();
        f.setFamily("IBM Plex Mono");
        f.setPixelSize(9);
        f.setBold(true);
        btn->setFont(f);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        btn->setProperty("trackMetaFilter", true);
        btn->setProperty("trackEventFlag", static_cast<qulonglong>(flag));
        button_row_layout->addWidget(btn, 0, col);
        m_track_event_filter_buttons.append(btn);
        connect(btn, &QToolButton::toggled, this, [this](bool) {
            applyTrackMetaMaskFromUi();
        });
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn](const QPoint &) {
            for (auto *other : m_track_event_filter_buttons) {
                if (!other) continue;
                const QSignalBlocker blocker(other);
                other->setChecked(other == btn);
            }
            applyTrackMetaMaskFromUi();
        });
    };

    // 1x6 compact row
    addFilterButton(0, "P", TrackEventView_Program, "Program change markers");
    addFilterButton(1, "V", TrackEventView_Volume, "Volume controller (CC7)");
    addFilterButton(2, "E", TrackEventView_Expression, "Expression controller (CC11)");
    addFilterButton(3, "N", TrackEventView_Pan, "Pan controller (CC10)");
    addFilterButton(4, "B", TrackEventView_Pitch, "Pitch bend");
    addFilterButton(5, "C", TrackEventView_ControlOther, "Other controller events");

    connect(m_track_event_preset_combo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        TrackEventViewMask preset_mask = presetMaskForIndex(idx);
        if (preset_mask == 0) return;
        for (auto *btn : m_track_event_filter_buttons) {
            if (!btn) continue;
            const TrackEventViewMask flag = static_cast<TrackEventViewMask>(btn->property("trackEventFlag").toULongLong());
            const QSignalBlocker blocker(btn);
            btn->setChecked((preset_mask & flag) != 0);
        }
        m_track_event_mask_ui = preset_mask;
        if (m_controller) {
            m_controller->setTrackEventPreset(idx);
        }
    });

    applyTrackMetaMaskFromUi();
}

void MainWindow::applyTrackMetaMaskFromUi() {
    if (m_track_event_filter_buttons.isEmpty()) return;

    TrackEventViewMask mask = 0;
    for (auto *btn : m_track_event_filter_buttons) {
        if (!btn) continue;
        if (!btn->isChecked()) continue;
        mask |= static_cast<TrackEventViewMask>(btn->property("trackEventFlag").toULongLong());
    }

    // Keep at least one filter active.
    if (mask == 0) {
        mask = kTrackEventView_All;
        for (auto *btn : m_track_event_filter_buttons) {
            if (!btn) continue;
            const QSignalBlocker blocker(btn);
            btn->setChecked(true);
        }
    }

    m_track_event_mask_ui = mask;
    if (m_track_event_preset_combo) {
        const int preset_idx = presetIndexForMask(mask);
        const QSignalBlocker blocker(m_track_event_preset_combo);
        m_track_event_preset_combo->setCurrentIndex(preset_idx);
    }
    if (m_controller) {
        m_controller->setTrackEventViewMask(static_cast<uint32_t>(mask));
    }
}

void MainWindow::syncSongListSelectionToOpenSong(bool scroll_to_center) {
    if (!ui || !ui->listView_songTable) return;
    auto *model = ui->listView_songTable->model();
    if (!model || m_config.recent_song.isEmpty()) return;

    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        QModelIndex idx = model->index(row, 0);
        if (!idx.isValid()) continue;
        if (idx.data(Qt::UserRole).toString() == m_config.recent_song) {
            ui->listView_songTable->setCurrentIndex(idx);
            if (scroll_to_center) {
                ui->listView_songTable->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            }
            return;
        }
    }
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
    }
    if (m_config.palette.isEmpty()) m_config.palette = "default";
    if (m_config.theme.isEmpty()) m_config.theme = "default";
    m_config.window_geometry = QString::fromUtf8(saveGeometry().toBase64());
    m_config.saveToFile(GtmConfig::defaultPath());
    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (m_controller && !m_config.recent_song.isEmpty()) {
        m_controller->selectSongByTitle(m_config.recent_song);
    }
}

void MainWindow::setRecentSongTitle(const QString &title) {
    m_config.recent_song = title;
}
