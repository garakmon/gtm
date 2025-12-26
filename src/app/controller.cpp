#include "controller.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QFrame>
#include <QGraphicsView>
#include <QScrollBar>

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
#include "minimapwidget.h"
#include "meters.h"


/// controller loads the song, creates the track items, etc.
Controller::Controller(MainWindow *window) : QObject(window) {
    this->m_window = window->ui;
    this->m_piano_roll = new PianoRoll(this);
    connect(this->m_piano_roll, &PianoRoll::eventItemSelected, this, &Controller::displayEvent);
    this->m_track_roll = new TrackRoll(this);
    connect(this->m_track_roll, &TrackRoll::trackMuteToggled, this, &Controller::onTrackMuteToggled);
    connect(this->m_track_roll, &TrackRoll::trackSoloToggled, this, &Controller::onTrackSoloToggled);
    this->m_measure_roll = new MeasureRoll(this);

    this->m_player = std::make_unique<Player>();
    if (!this->m_player->initializeAudio()) {
        qDebug() << "Failed to initialize audio.";
    }

    connect(&this->m_player_timer, &QTimer::timeout, this, &Controller::syncRolls);

    this->setupRolls();
    if (window) {
        setMinimap(window->m_minimap);
    }
}

void Controller::setupRolls() {
    // // scrolling should be synced, and this is accomplished by sharing a scroll bar between views
    m_window->view_piano->setVerticalScrollBar(m_window->vscroll_pianoRoll);
    m_window->view_pianoRoll->setVerticalScrollBar(m_window->vscroll_pianoRoll);

    m_window->view_trackList->setVerticalScrollBar(m_window->vscroll_trackRoll);
    m_window->view_trackRoll->setVerticalScrollBar(m_window->vscroll_trackRoll);

    // Sync horizontal scrolling
    m_window->view_pianoRoll->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_measures_tracks->setHorizontalScrollBar(m_window->hscroll_pianoRoll);
    m_window->view_trackRoll->setHorizontalScrollBar(m_window->hscroll_pianoRoll);

    // Configure policies (Only need to do this once)
    m_window->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_window->view_pianoRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_pianoRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_window->view_trackList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_trackList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_window->view_measures_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setFrameShape(QFrame::NoFrame);

    m_window->view_measures->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setFrameShape(QFrame::NoFrame);

    m_window->view_piano->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_pianoRoll->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_trackList->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_trackRoll->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_measures->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_measures_tracks->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    m_window->view_piano->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_pianoRoll->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_trackList->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_trackRoll->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_measures->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_measures_tracks->setCacheMode(QGraphicsView::CacheBackground);

    // Hide horizontal scrollbar; minimap will drive scrolling.
    m_window->hscroll_pianoRoll->setVisible(false);

    m_window->view_measures->viewport()->installEventFilter(this);
    m_window->view_pianoRoll->viewport()->installEventFilter(this);
    m_window->view_trackRoll->viewport()->installEventFilter(this);

    m_scroll_debounce.setSingleShot(true);
    m_scroll_debounce.setInterval(400);

    // Hide vertical scrollbars; minimap will handle horizontal, and mouse wheel can pan vertically.
    m_window->vscroll_pianoRoll->setVisible(false);
    m_window->vscroll_trackRoll->setVisible(false);
}

void Controller::displayRolls() {
    this->m_piano_roll->display();

    // 1. Piano View
    m_window->view_piano->setScene(m_piano_roll->scenePiano());
    QRectF piano_bounds = m_piano_roll->scenePiano()->itemsBoundingRect();
    m_window->view_piano->setSceneRect(0.0, 0.0, piano_bounds.width(), piano_bounds.height());
    m_window->view_piano->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_piano->setRenderHint(QPainter::Antialiasing, false);
    // Keep piano keys background as-is (piano scene draws its own items)
    m_window->view_piano->setBackgroundBrush(QBrush(QColor(0x282828), Qt::SolidPattern));

    // 2. Piano Roll View (Notes) - Stickied to Top/Left
    m_window->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    QRectF roll_bounds = m_piano_roll->sceneRoll()->sceneRect();
    m_window->view_pianoRoll->setSceneRect(roll_bounds);
    m_window->view_pianoRoll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_pianoRoll->setBackgroundBrush(Qt::NoBrush);

    // 3. Track Roll (Sticky Top/Left)
    m_window->view_trackRoll->setScene(m_track_roll->sceneRoll());
    QRectF track_roll_bounds = m_track_roll->sceneRoll()->itemsBoundingRect();
    m_window->view_trackRoll->setSceneRect(0.0, 0.0, track_roll_bounds.width(), track_roll_bounds.height());
    m_window->view_trackRoll->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // 4. Track List (Sticky Top/Left)
    m_window->view_trackList->setScene(m_track_roll->sceneTracks());
    QRectF track_list_bounds = m_track_roll->sceneTracks()->itemsBoundingRect();
    m_window->view_trackList->setSceneRect(0.0, 0.0, track_list_bounds.width(), track_list_bounds.height());
    m_window->view_trackList->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // 5. Measures (Sticky Top/Left)
    QRectF measure_bounds = m_measure_roll->sceneMeasures()->itemsBoundingRect();

    m_window->view_measures_tracks->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures_tracks->setSceneRect(0.0, 0.0, measure_bounds.width(), ui_measure_roll_height);
    m_window->view_measures_tracks->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_window->view_measures->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures->setSceneRect(0.0, 0.0, measure_bounds.width(), ui_measure_info_height);
    m_window->view_measures->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    if (m_minimap) {
        m_minimap->setSong(m_song);
        m_minimap->setScrollBar(m_window->hscroll_pianoRoll);
        m_minimap->setView(m_window->view_pianoRoll);
    }

    // Navigation
    if (!m_piano_roll->keys().isEmpty()) {
        m_window->view_piano->centerOn(m_piano_roll->keys()[g_midi_middle_c]);
    }

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
    QString title = index.data(Qt::UserRole).toString();

    SongEntry entry = this->m_project->getSongEntryByTitle(title);

    // std::shared_ptr<Song> song = this->m_project->getSong(title);
    // !TODO: load unloaded songs here (well, in the project function)

    qDebug() << "Clicked" << title;
    qDebug() << "voicegroup:" << entry.voicegroup << "player:" << entry.player << "file:" << entry.midifile;

    //this->m_project->addSong(title);

    if (!this->m_project->songLoaded(title)) {
        smf::MidiFile midi = this->m_interface->loadMidi(title);
        this->m_project->addSong(title, midi);
    }

    //std::shared_ptr<Song> song = this->m_project->getSong(title);
    this->m_project->setActiveSong(title);

    this->loadSong();

    // steps: load song into project, set it active, call loadSong()
}

void Controller::syncRolls() {
    if (!this->m_song) {
        return;
    }
    double current_seconds = m_player->getSequencer()->getCurrentTime();

    QTime display_time(0, 0);
    display_time = display_time.addMSecs(static_cast<int>(current_seconds * 1000));
    QString time_text = display_time.toString("mm:ss.zzz");
    this->m_window->LCD_Timer->display(time_text);

    int current_tick = this->m_song->getTickFromTime(current_seconds);

    this->m_measure_roll->updatePlaybackGuide(current_tick);
    this->m_measure_roll->setTick(current_tick);

    // Update track items with current instrument info
    Mixer *mixer = m_player->getMixer();

    int playhead_x = current_tick * ui_tick_x_scale;
    int viewport_left = this->m_window->hscroll_pianoRoll->value();
    int viewport_width = this->m_window->view_measures->viewport()->width();
    int activation_point = viewport_left + viewport_width / 4;

    if (!m_autoscroll_enabled && !m_scroll_debounce.isActive() && playhead_x >= activation_point) {
        m_autoscroll_enabled = true;
    }

    if (m_autoscroll_enabled) {
        int scroll_value = playhead_x - viewport_width / 4;
        this->m_window->hscroll_pianoRoll->setValue(scroll_value);
    }

    if (current_tick >= this->m_song->durationInTicks()) {
        this->stop();
    }

    // Update meters at UI tick rate
    if (mixer) {
        m_meter_frame = (m_meter_frame + 1) & 1;
        if (m_meter_frame == 0) {
            Mixer::MeterLevels levels;
            mixer->getMeterLevels(&levels);
            if (m_master_meter) {
                m_master_meter->setLevels(levels.master_l, levels.master_r);
            }
            int first_row = 0;
            int last_row = g_num_midi_channels - 1;
            if (m_window && m_window->view_trackList) {
                int scroll_y = m_window->view_trackList->verticalScrollBar()->value();
                int viewport_h = m_window->view_trackList->viewport()->height();
                first_row = qMax(0, scroll_y / ui_track_item_height);
                last_row = qMin(g_num_midi_channels - 1,
                                first_row + (viewport_h / ui_track_item_height) + 1);
            }

            for (int ch = first_row; ch <= last_row; ++ch) {
                auto info = mixer->getChannelPlayInfo(ch);
                if (info.active) {
                    m_track_roll->setTrackPlayingInfo(ch, info.voice_type);
                } else {
                    m_track_roll->setTrackPlayingInfo(ch, QString());
                }
                m_track_roll->setTrackMeterLevels(ch, levels.channel_l[ch], levels.channel_r[ch]);
            }
        }
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
        return true;
    }
    return false;
}

bool Controller::loadSong() {
    return this->loadSong(this->m_project->activeSong());
}

bool Controller::loadSong(std::shared_ptr<Song> song) {
    this->stop();

    song->load();

    this->m_song = song;

    this->m_track_roll->setSong(song);
    this->m_piano_roll->setSong(song);
    this->m_measure_roll->setSong(song);

    this->displayRolls();

    // reset playhead to start
    this->m_measure_roll->setTick(0);
    this->m_measure_roll->updatePlaybackGuide(0);
    this->m_window->LCD_Timer->display("00:00.000");

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
    const VoiceGroup *voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *all_voicegroups = nullptr;
    const QMap<QString, Sample> *samples = nullptr;
    const QMap<QString, QByteArray> *pcm_data = nullptr;
    const QMap<QString, KeysplitTable> *keysplit_tables = nullptr;

    if (m_song && m_project) {
        voicegroup = m_project->getVoiceGroup(m_song->getMetaInfo().voicegroup);
        all_voicegroups = &m_project->getVoiceGroups();
        samples = &m_project->getSamples();
        pcm_data = &m_project->getPcmData();
        keysplit_tables = &m_project->getKeysplitTables();
    }

    m_player->loadSong(m_song.get(), voicegroup, all_voicegroups, samples, pcm_data, keysplit_tables);

    m_playback_start_tick = m_measure_roll->tick();
    m_player->seekToTick(m_playback_start_tick);

    m_autoscroll_enabled = true;
    m_player->play();

    m_player_elapsed.start();
    m_player_timer.start(16);
}

void Controller::stop() {
    m_player->stop();
    m_player_timer.stop();
    m_track_roll->clearAllPlayingInfo();
    m_track_roll->clearAllMeters();
    if (m_master_meter) {
        m_master_meter->setLevels(0.0f, 0.0f);
    }
}

void Controller::pause() {
    m_player->stop();
    m_player_timer.stop();
    m_track_roll->clearAllPlayingInfo();
    m_track_roll->clearAllMeters();
    if (m_master_meter) {
        m_master_meter->setLevels(0.0f, 0.0f);
    }
}

void Controller::onTrackMuteToggled(int channel, bool muted) {
    // Mute is non-exclusive. If any solo is active, clear it and keep current mute state.
    if (m_track_roll->hasSoloed()) {
        m_track_roll->clearAllSoloed();
    }

    m_track_roll->setTrackMuted(channel, muted);
    m_player->getMixer()->setChannelMute(channel, muted);
}

void Controller::onTrackSoloToggled(int channel, bool soloed) {
    if (soloed) {
        // Solo mutes all other tracks, and clears any other solo state.
        m_track_roll->clearAllSoloed();
        m_track_roll->setTrackSoloed(channel, true);

        m_player->getMixer()->setAllMuted(true);
        m_player->getMixer()->setChannelMute(channel, false);

        // Reflect in UI mute states.
        for (int ch = 0; ch < g_num_midi_channels; ch++) {
            m_track_roll->setTrackMuted(ch, ch != channel);
        }
    } else {
        // Clearing solo unmutes all tracks.
        m_track_roll->clearAllSoloed();
        m_player->getMixer()->setAllMuted(false);
        for (int ch = 0; ch < g_num_midi_channels; ch++) {
            m_track_roll->setTrackMuted(ch, false);
            m_player->getMixer()->setChannelMute(ch, false);
        }
    }
}

void Controller::setMinimap(MinimapWidget *minimap) {
    m_minimap = minimap;
}

void Controller::setMasterMeter(MasterMeterWidget *meter) {
    m_master_meter = meter;
}

void Controller::setMasterVolume(int value) {
    if (!m_player) return;
    float volume = static_cast<float>(value) / 100.0f;
    m_player->getMixer()->setMasterVolume(volume);
}

void Controller::seekToTick(int tick) {
    bool was_playing = m_player_timer.isActive();

    m_measure_roll->setTick(tick);
    m_measure_roll->updatePlaybackGuide(tick);
    m_autoscroll_enabled = true;

    if (was_playing) {
        m_playback_start_tick = tick;
        m_player->seekToTick(tick);
        m_player_elapsed.restart();
    }
}

void Controller::seekToStart() {
    seekToTick(0);
}

int Controller::currentTick() const {
    if (!m_song) return 0;
    double current_seconds = m_player->getSequencer()->getCurrentTime();
    return m_song->getTickFromTime(current_seconds);
}

bool Controller::selectSongByIndex(int index) {
    if (!m_window || !m_window->listView_songTable) return false;
    auto *view = m_window->listView_songTable;
    int rows = view->model() ? view->model()->rowCount() : 0;
    if (rows <= 0) return false;
    if (index < 0 || index >= rows) return false;

    QModelIndex current = view->currentIndex();
    int column = current.isValid() ? current.column() : 0;
    QModelIndex next = view->model()->index(index, column);
    if (!next.isValid()) return false;

    view->setCurrentIndex(next);
    songListSongRequested(next);
    return true;
}

bool Controller::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->angleDelta().x() != 0) {
            m_autoscroll_enabled = false;
            m_scroll_debounce.start();
        }
    }

    if (watched == m_window->view_measures->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);

        if (mouse_event->button() == Qt::LeftButton && m_song) {
            QPointF scene_pos = m_window->view_measures->mapToScene(mouse_event->pos());

            int tick = static_cast<int>(scene_pos.x() / ui_tick_x_scale);
            int tpqn = m_song->getTicksPerQuarterNote();
            int snapped_tick = qRound(static_cast<double>(tick) / tpqn) * tpqn;
            snapped_tick = qBound(0, snapped_tick, m_song->durationInTicks());

            seekToTick(snapped_tick);
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}
