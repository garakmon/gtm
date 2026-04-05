#include "ui/mainwindow.h"

#include "app/songeditor.h"
#include "app/controller.h"
#include "ui/colors.h"
#include "ui/customwidgets.h"
#include "ui/dialogs.h"
#include "ui/graphicstrackitem.h"
#include "ui/meters.h"
#include "ui/minimapwidget.h"
#include "ui/pianoroll.h"
#include "ui/previewsoundwindow.h"
#include "ui/trackroll.h"
#include "ui/voicegroupeditor.h"
#include "ui_mainwindow.h"
#include "util/constants.h"
#include "util/logging.h"
#include "util/theme.h"

#include <QButtonGroup>
#include <QAction>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFontMetrics>
#include <QGraphicsEllipseItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QKeySequence>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <limits>



static TrackEventViewMask presetMaskForIndex(int index) {
    switch (index) {
    case static_cast<int>(TrackRoll::TrackEventPreset::All):
        return TrackEventView_All;
    case static_cast<int>(TrackRoll::TrackEventPreset::Mix):
        return TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan;
    case static_cast<int>(TrackRoll::TrackEventPreset::Timbre):
        return TrackEventView_Program;
    case static_cast<int>(TrackRoll::TrackEventPreset::Other):
        return TrackEventView_ControlOther;
    default:
        return 0;
    }
}

static int presetIndexForMask(TrackEventViewMask mask) {
    if (mask == TrackEventView_All) {
        return static_cast<int>(TrackRoll::TrackEventPreset::All);
    }
    if (mask == (TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan)) {
        return static_cast<int>(TrackRoll::TrackEventPreset::Mix);
    }
    if (mask == TrackEventView_Program) {
        return static_cast<int>(TrackRoll::TrackEventPreset::Timbre);
    }
    if (mask == TrackEventView_ControlOther) {
        return static_cast<int>(TrackRoll::TrackEventPreset::Other);
    }
    return static_cast<int>(TrackRoll::TrackEventPreset::Custom);
}

/**
 * Construct the window by setting up the ui, and restoring state from the config.
 * Attempts to load a project from the config as well.
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // !TODO: silence bug in my qt version re: mac trackpads [fix: update Qt version]
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));

    this->loadConfig();

    if (!m_config.window_geometry.isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(m_config.window_geometry.toUtf8()));
    }

    this->updateWindowTitle();
    this->loadProject();
}

MainWindow::~MainWindow() {
    delete ui;
}

/**
 * Coordinate startup using some setup helpers.
 */
void MainWindow::setupUi() {
    this->setupController();
    this->setupTrackMetaControls();
    this->setupEventEditorRanges();
    this->setupMinimap();
    this->setupMasterMeter();
    this->setupSongSearch();
    this->setupToolButtons();
    this->setupMetaDisplay();
    this->setupSplitterControls();
}

/**
 * Load values from the global config file and apply themes, etc.
 */
void MainWindow::loadConfig() {
    bool config_ok = false;
    m_config = GtmConfig::loadFromFile(GtmConfig::defaultPath(), &config_ok);

    if (m_config.palette.isEmpty()) {
        m_config.palette = "default";
    }
    if (m_config.theme.isEmpty()) {
        m_config.theme = "default";
    }

    ThemePalette palette = paletteByName(m_config.palette);
    applyTheme(palette, m_config.theme);
    applyThemeColors(palette);

    if (m_master_meter && m_master_meter->slider()) {
        int volume = qBound(0, m_config.master_volume, 100);
        m_master_meter->slider()->setValue(volume);
    }
}

/**
 * Creates the Controller, connects the song-selected signal back into window state
 * updates, and applies the equal stretch policy to the Song/Track/Event groupboxes.
 */
void MainWindow::setupController() {
    m_controller = std::make_unique<Controller>(this);
    connect(m_controller.get(), &Controller::songSelected, this,
        [this](const QString &title) {
            this->setRecentSongTitle(title);
            this->syncSongListSelectionToOpenSong(false);
            this->updateWindowTitle();
    });

    if (ui->verticalLayout_InfoPanels) {
        ui->verticalLayout_InfoPanels->setStretch(0, 1); // Song Info
        ui->verticalLayout_InfoPanels->setStretch(1, 1); // Track Info
        ui->verticalLayout_InfoPanels->setStretch(2, 1); // Event Info
    }
}

/**
 * Initialize the default valid ranges for the event editor spinboxes.
 */
void MainWindow::setupEventEditorRanges() {
    if (ui->spinBox_NoteKey) {
        ui->spinBox_NoteKey->setRange(0, 0xff);
    }
    if (ui->spinBox_NoteChannel) {
        ui->spinBox_NoteChannel->setRange(0, 0xf);
    }
    if (ui->spinBox_NoteOnVelocity) {
        ui->spinBox_NoteOnVelocity->setRange(0, 0xff);
    }
    if (ui->spinBox_NoteOffVelocity) {
        ui->spinBox_NoteOffVelocity->setRange(0, 0xff);
    }
    if (ui->spinBox_NoteOnTick) {
        ui->spinBox_NoteOnTick->setRange(0, std::numeric_limits<int>::max());
    }
    if (ui->spinBox_NoteOffTick) {
        ui->spinBox_NoteOffTick->setRange(0, std::numeric_limits<int>::max());
    }
}

/**
 * Give the Controller access to the minimap widget.
 */
void MainWindow::setupMinimap() {
    if (!ui->widget_minimap) return;

    m_minimap = ui->widget_minimap;
    m_minimap->setObjectName("minimap");
    if (m_controller) {
        m_controller->setMinimap(m_minimap);
    }
}

/**
 * The master meter controls the master volume.
 * It gets given to the Controller and its value is kept in the config.
 * This function also adjusts the megaphone icon based on the volume level.
 */
void MainWindow::setupMasterMeter() {
    if (ui->widget_masterMeter) {
        m_master_meter = ui->widget_masterMeter;
        m_master_meter->setObjectName("masterMeter");
        if (auto *layout = m_master_meter->parentWidget() ?
                           m_master_meter->parentWidget()->layout() : nullptr) {
            layout->setAlignment(Qt::AlignVCenter);
        }
        if (m_controller) {
            m_controller->setMasterMeter(m_master_meter);
            connect(m_master_meter->slider(), &QSlider::valueChanged,
                    m_controller.get(), &Controller::setMasterVolume);
            m_controller->setMasterVolume(m_master_meter->slider()->value());
        }
        if (m_master_meter->slider()) {
            connect(m_master_meter->slider(), &QSlider::valueChanged, this,
                [this](int value) {
                    m_config.master_volume = value;
            });
        }
    }

    if (ui->label_volume) {
        GTMSvgIconWidget *icon = ui->label_volume;
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
}

/**
 * Configures the song list search widget, including its leading icon and filter behavior.
 */
void MainWindow::setupSongSearch() {
    if (ui->lineEdit_SongList_search) {
        GTMLineEdit *search = ui->lineEdit_SongList_search;
        search->setLeadingSvg(":/icons/search-engine.svg", 14);
        connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (!m_song_filter) return;
            if (text.trimmed().isEmpty()) {
                m_song_filter->setFilterRegularExpression(QRegularExpression());
                this->syncSongListSelectionToOpenSong(true);
                return;
            }
            QRegularExpression regex(QRegularExpression::escape(text),
                                  QRegularExpression::CaseInsensitiveOption);
            m_song_filter->setFilterRegularExpression(regex);
        });
    }
}

/**
 * Sets up the editing tool button exclusivity and makes the playback buttons checkable.
 */
void MainWindow::setupToolButtons() {
    auto update_track_tools_enabled = [this]() {
        if (!ui->pillbox_TrackTools) {
            return;
        }

        bool selection_tool_active = false;
        if (ui->pillbox_SelectionTools) {
            const QList<QToolButton *> selection_buttons =
                ui->pillbox_SelectionTools->findChildren<QToolButton *>();
            for (QToolButton *button : selection_buttons) {
                if (button && button->isChecked()) {
                    selection_tool_active = true;
                    break;
                }
            }
        }

        ui->pillbox_TrackTools->setEnabled(selection_tool_active);
    };

    QButtonGroup *edit_tool_group = new QButtonGroup(this);
    edit_tool_group->setExclusive(true);

    if (ui->pillbox_SelectionTools) {
        const QList<QToolButton *> selection_buttons =
            ui->pillbox_SelectionTools->findChildren<QToolButton *>();
        for (QToolButton *button : selection_buttons) {
            if (button == ui->button_select_invert) {
                button->setCheckable(false);
                button->setChecked(false);
                continue;
            }
            button->setCheckable(true);
            edit_tool_group->addButton(button);
        }
    }

    if (ui->pillbox_NoteTools) {
        const QList<QToolButton *> note_buttons =
            ui->pillbox_NoteTools->findChildren<QToolButton *>();
        for (QToolButton *button : note_buttons) {
            button->setCheckable(true);
            edit_tool_group->addButton(button);
        }
    }

    if (ui->pillbox_MetaTools) {
        const QList<QToolButton *> meta_buttons =
            ui->pillbox_MetaTools->findChildren<QToolButton *>();
        for (QToolButton *button : meta_buttons) {
            button->setCheckable(true);
            edit_tool_group->addButton(button);
        }
    }

    if (ui->button_select_normal) {
        ui->button_select_normal->setChecked(true);
    } else if (ui->button_select_rect) {
        ui->button_select_rect->setChecked(true);
    }

    connect(edit_tool_group, &QButtonGroup::buttonToggled, this,
        [update_track_tools_enabled](QAbstractButton *, bool) {
            update_track_tools_enabled();
    });
    update_track_tools_enabled();

    if (ui->pillbox_TrackTools) {
        const QList<QToolButton *> track_buttons =
            ui->pillbox_TrackTools->findChildren<QToolButton *>();
        for (QToolButton *button : track_buttons) {
            button->setCheckable(false);
            button->setChecked(false);
        }
    }

    if (ui->toolButton) {
        ui->toolButton->setEnabled(false);
    }

    if (ui->button_note_quantize) {
        ui->button_note_quantize->setEnabled(false);
    }

    if (ui->button_note_trim) {
        ui->button_note_trim->setEnabled(false);
    }

    if (ui->button_tracks_merge) {
        ui->button_tracks_merge->setEnabled(false);
    }

    if (ui->button_tracks_split) {
        ui->button_tracks_split->setEnabled(false);
    }

    if (ui->Button_Play) ui->Button_Play->setCheckable(true);
    if (ui->Button_Pause) ui->Button_Pause->setCheckable(true);
    if (ui->Button_Stop) ui->Button_Stop->setCheckable(true);

    if (ui->button_AddSong) {
        connect(ui->button_AddSong, &QPushButton::clicked, this, [this]() {
            NewSongDialog dialog(this);
            if (m_controller) {
                dialog.setVoicegroups(m_controller->voicegroupNames());
                dialog.setPlayers(m_controller->playerNames());
                dialog.setExistingSongTitles(m_controller->songTitles());
            }
            if (dialog.exec() == QDialog::Accepted) {
                const NewSongSettings settings = dialog.result();
                QString error;
                if (!m_controller->createSong(settings, &error)) {
                    QMessageBox::warning(this, "Create Song Failed",
                                         error.isEmpty() ? "Failed to create song." : error);
                }
            }
        });
    }
}

/**
 * Applies sizing and alignment rules to the meta display labels
 * so value changes do not cause layout wonkiness.
 */
void MainWindow::setupMetaDisplay() {
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
        ui->label_MetaTimeValue->setSizePolicy(QSizePolicy::Preferred,
                                               QSizePolicy::Expanding);
        QFontMetrics fm(ui->label_MetaTimeValue->font());
        ui->label_MetaTimeValue->setMinimumWidth(fm.horizontalAdvance("00:00.000") + 8);
    }
    setFixedLabelWidth(ui->label_MetaMeasureBeatValue, "123.16");
    setFixedLabelWidth(ui->label_MetaTempoValue, "240.0 BPM");
    setFixedLabelWidth(ui->label_MetaTimeSigValue, "12/16");
    setFixedLabelWidth(ui->label_MetaKeyValue, "Key Sig");
}

/**
 * Wires the Notes|Events|All radio buttons to the splitter,
 * and manages pane visibility plus saved splitter sizes.
 */
void MainWindow::setupSplitterControls() {
    auto updateSplitterVisibility = [this]() {
        if (!ui->splitter || !ui->scrollArea || !ui->scroll_pianoArea) return;
        if (ui->scrollArea->isVisible() && ui->scroll_pianoArea->isVisible()) {
            m_splitter_sizes = ui->splitter->sizes();
        }
        const bool show_all = ui->radioButton_pianoAll
                           && ui->radioButton_pianoAll->isChecked();
        const bool show_events = show_all || (ui->radioButton_pianoEvents
                                           && ui->radioButton_pianoEvents->isChecked());
        const bool show_notes = show_all || (ui->radioButton_pianoNotes
                                           && ui->radioButton_pianoNotes->isChecked());

        ui->scrollArea->setVisible(show_events);
        ui->scroll_pianoArea->setVisible(show_notes);

        int total = ui->splitter->height();
        if (total <= 0) total = 1;
        if (show_events && show_notes) {
            if (m_splitter_sizes.size() == 2
             && (m_splitter_sizes[0] + m_splitter_sizes[1]) > 0) {
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
        connect(ui->radioButton_pianoAll, &QRadioButton::toggled,
                this, updateSplitterVisibility);
    }
    if (ui->radioButton_pianoEvents) {
        connect(ui->radioButton_pianoEvents, &QRadioButton::toggled,
                this, updateSplitterVisibility);
    }
    if (ui->radioButton_pianoNotes) {
        connect(ui->radioButton_pianoNotes, &QRadioButton::toggled,
                this, updateSplitterVisibility);
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
}

/**
 * Try to load the most recent project from the config file.
 */
void MainWindow::loadProject() {
    if (m_config.most_recent_project.isEmpty()) {
        return;
    }

    this->loadProject(m_config.most_recent_project);
}

/**
 * Load the project and intialize the sort filter model.
 */
void MainWindow::loadProject(const QString &root) {
    if (root.isEmpty()) {
        return;
    }

    this->m_controller->loadProject(root);
    m_config.most_recent_project = root;
    this->setupEditMenuActions();

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
}

void MainWindow::setupEditMenuActions() {
    if (!ui || !ui->menuEdit || !m_controller) return;

    SongEditor *editor = m_controller->songEditor();
    if (!editor) return;

    QUndoGroup *history_group = editor->historyGroup();
    if (!history_group) return;

    if (!m_action_undo) {
        m_action_undo = history_group->createUndoAction(this, "Undo");
        m_action_undo->setShortcut(QKeySequence::Undo);
        ui->menuEdit->addAction(m_action_undo);
    }
    if (!m_action_redo) {
        m_action_redo = history_group->createRedoAction(this, "Redo");
        m_action_redo->setShortcut(QKeySequence::Redo);
        ui->menuEdit->addAction(m_action_redo);
    }
    if (!m_action_delete_selected) {
        m_action_delete_selected = new QAction("Delete Selected Notes", this);
        m_action_delete_selected->setShortcuts(
            QList<QKeySequence>{QKeySequence(Qt::Key_Delete),
                                QKeySequence(Qt::Key_Backspace)}
        );
        // keep delete scoped to the roll areas so other widgets (eg. text inputs) still work
        m_action_delete_selected->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(m_action_delete_selected, &QAction::triggered, this, [this]() {
            if (!m_controller) {
                return;
            }

            QString error;
            if (!m_controller->deleteSelectedEvents(&error) && !error.isEmpty()) {
                logging::warn("Failed to delete selected events: " + error,
                              logging::LogCategory::Ui);
            }
        });
        ui->menuEdit->addAction(m_action_delete_selected);
        if (ui->splitter) {
            ui->splitter->addAction(m_action_delete_selected);
        }
    }
}

/**
 * Selects the currently open song in the song list view and optionally centers it.
 */
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

void MainWindow::setRecentSongTitle(const QString &title) {
    m_config.recent_song = title;
}

void MainWindow::updateWindowTitle() {
    QString title = "gtm";
    if (!m_config.recent_song.isEmpty()) {
        title += QString(" - %1").arg(m_config.recent_song);
    }
    if (m_controller && m_controller->hasUnsavedChanges()) {
        title += " *";
    }
    this->setWindowTitle(title);
}

/**
 * Sets up the track-meta preset combo and buttons inside the track-meta controls frame.
 */
void MainWindow::setupTrackMetaControls() {
    if (!ui || !ui->frame_TrackMetaControls) return;

    // rebuild any existing controls
    if (auto *old_layout = ui->frame_TrackMetaControls->layout()) {
        QLayoutItem *child = nullptr;
        while ((child = old_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        delete old_layout;
    }

    // create the root layout
    auto *root_layout = new QVBoxLayout(ui->frame_TrackMetaControls);
    root_layout->setContentsMargins(1, 1, 1, 1);
    root_layout->setSpacing(1);

    // create the preset selector
    m_track_event_preset_combo = new GTMComboBox(ui->frame_TrackMetaControls);
    m_track_event_preset_combo->addItem("All");
    m_track_event_preset_combo->addItem("Mix");
    m_track_event_preset_combo->addItem("Timbre");
    m_track_event_preset_combo->addItem("Other");
    m_track_event_preset_combo->addItem("Custom");
    m_track_event_preset_combo->setCurrentIndex(
        static_cast<int>(TrackRoll::TrackEventPreset::All)
    );
    m_track_event_preset_combo->setMinimumHeight(16);
    m_track_event_preset_combo->setMaximumHeight(16);
    root_layout->addWidget(m_track_event_preset_combo);

    // create the filter button row
    auto *button_row_layout = new QGridLayout();
    button_row_layout->setContentsMargins(0, 0, 0, 0);
    button_row_layout->setHorizontalSpacing(1);
    button_row_layout->setVerticalSpacing(0);
    root_layout->addLayout(button_row_layout);

    // helper for the filter buttons
    auto addFilterButton = [this, button_row_layout](int col, const QString &text,
                                                     TrackEventViewMask flag,
                                                     const QString &tooltip) {
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
            this->applyTrackMetaMaskFromUi();
        });
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QToolButton::customContextMenuRequested, this,
            [this, btn](const QPoint &) {
                for (QToolButton *other : m_track_event_filter_buttons) {
                    if (!other) continue;
                    const QSignalBlocker blocker(other);
                    other->setChecked(other == btn);
                }
                this->applyTrackMetaMaskFromUi();
            });
    };

    // populate the filter row
    addFilterButton(0, "P", TrackEventView_Program, "Program change markers");
    addFilterButton(1, "V", TrackEventView_Volume, "Volume controller (CC7)");
    addFilterButton(2, "E", TrackEventView_Expression, "Expression controller (CC11)");
    addFilterButton(3, "N", TrackEventView_Pan, "Pan controller (CC10)");
    addFilterButton(4, "B", TrackEventView_Pitch, "Pitch bend");
    addFilterButton(5, "C", TrackEventView_ControlOther, "Other controller events");

    // apply presets through the combo box
    connect(m_track_event_preset_combo, &QComboBox::currentIndexChanged,
        this, [this](int index) {
            TrackEventViewMask preset_mask = presetMaskForIndex(index);
            if (preset_mask == 0) return;
            for (QToolButton *btn : m_track_event_filter_buttons) {
                if (!btn) continue;
                const TrackEventViewMask flag = static_cast<TrackEventViewMask>(
                    btn->property("trackEventFlag").toULongLong()
                );
                const QSignalBlocker blocker(btn);
                btn->setChecked((preset_mask & flag) != 0);
            }
            m_track_event_mask_ui = preset_mask;
            if (m_controller) {
                m_controller->setTrackEventPreset(index);
            }
        });

    // sync initial ui state
    this->applyTrackMetaMaskFromUi();
}

/**
 * Read the track-meta filter button state, normalize it, update the preset combo,
 * and forward the resulting mask to the controller.
 */
void MainWindow::applyTrackMetaMaskFromUi() {
    if (m_track_event_filter_buttons.isEmpty()) return;

    TrackEventViewMask mask = 0;
    for (QToolButton *btn : m_track_event_filter_buttons) {
        if (!btn) continue;
        if (!btn->isChecked()) continue;
        mask |= static_cast<TrackEventViewMask>(
            btn->property("trackEventFlag").toULongLong()
        );
    }

    // Keep at least one filter active.
    if (mask == 0) {
        mask = TrackEventView_All;
        for (QToolButton *btn : m_track_event_filter_buttons) {
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

/**
 * On window show, attempt to reopen the configed recent song.
 */
void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (m_controller && !m_config.recent_song.isEmpty()) {
        if (m_controller->selectSongByTitle(m_config.recent_song)) {
            this->syncSongListSelectionToOpenSong(true);
        }
    }
}

/**
 * Saves final states into the config file before closing the window.
 */
void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_controller && !m_controller->projectRoot().isEmpty()) {
        m_config.most_recent_project = m_controller->projectRoot();
    }
    if (m_config.palette.isEmpty()) m_config.palette = "default";
    if (m_config.theme.isEmpty()) m_config.theme = "default";
    m_config.window_geometry = QString::fromUtf8(saveGeometry().toBase64());
    m_config.saveToFile(GtmConfig::defaultPath());
    QMainWindow::closeEvent(event);
}

void MainWindow::on_action_Open_triggered() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"), ".",
                            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!path.isEmpty()) {
        this->loadProject(path);
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

/**
 * Opens the sound preview window, probably going to be a scrapped feature.
 */
void MainWindow::on_action_PreviewSound_triggered() {
    if (m_preview_sound_window) {
        m_preview_sound_window->show();
    }
    else {
        m_preview_sound_window = new PreviewSoundWindow(this);
        m_preview_sound_window->setAttribute(Qt::WA_DeleteOnClose);
        m_preview_sound_window->show();
    }
}

void MainWindow::on_action_VoicegroupEditor_triggered() {
    if (m_voicegroup_editor_window && m_controller) {
        m_voicegroup_editor_window->setVoicegroups(m_controller->voicegroups());
        m_voicegroup_editor_window->setKeysplitTables(m_controller->keysplitTables());
    }

    if (m_voicegroup_editor_window) {
        m_voicegroup_editor_window->show();
        m_voicegroup_editor_window->raise();
        m_voicegroup_editor_window->activateWindow();
    }
    else {
        m_voicegroup_editor_window = new VoicegroupEditor(this);
        if (m_controller) {
            m_voicegroup_editor_window->setVoicegroups(m_controller->voicegroups());
            m_voicegroup_editor_window->setKeysplitTables(m_controller->keysplitTables());
        }
        m_voicegroup_editor_window->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_voicegroup_editor_window, &QObject::destroyed, this, [this]() {
            m_voicegroup_editor_window = nullptr;
        });
        m_voicegroup_editor_window->show();
    }
}
