#include "controller.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QFrame>
#include <QGraphicsView>
#include <QScrollBar>
#include <QMessageBox>
#include <QLabel>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cmath>

#include "../ui/ui_mainwindow.h"
#include "mainwindow.h"
#include "pianoroll.h"
#include "trackroll.h"
#include "measureroll.h"
#include "constants.h"
#include "colors.h"
#include "util.h"
#include "song.h"
#include "MidiEvent.h"
#include "project.h"
#include "projectinterface.h"
#include "songlistmodel.h"
#include "minimapwidget.h"
#include "meters.h"

namespace {
uint8_t programForEvent(const Song *song, const smf::MidiEvent *event) {
    if (!song || !event) return 0;
    const uint8_t channel = event->getChannel();
    uint8_t program = song->getInitialPrograms()[channel];
    const int track = event->track;
    auto tracks = song->tracks();
    if (track >= 0 && track < static_cast<int>(tracks.size())) {
        smf::MidiEventList *list = tracks[track];
        if (list) {
            const int count = list->getEventCount();
            for (int i = 0; i < count; ++i) {
                smf::MidiEvent &ev = (*list)[i];
                if (ev.tick > event->tick) break;
                if (ev.isPatchChange() && ev.getChannel() == channel) {
                    program = ev.getP1();
                }
            }
        }
    }
    return program;
}

const Instrument *resolveInstrumentForEvent(const Instrument *inst,
                                            uint8_t key,
                                            const QMap<QString, VoiceGroup> *all_voicegroups,
                                            const QMap<QString, KeysplitTable> *keysplit_tables) {
    if (!inst || !all_voicegroups) return inst;

    if (inst->type_id == 0x40) { // voice_keysplit
        auto vg_it = all_voicegroups->find(inst->sample_label);
        if (vg_it == all_voicegroups->end()) return nullptr;

        int inst_index = 0;
        if (keysplit_tables && !inst->keysplit_table.isEmpty()) {
            auto table_it = keysplit_tables->find(inst->keysplit_table);
            if (table_it != keysplit_tables->end()) {
                const KeysplitTable &table = table_it.value();
                auto note_it = table.note_map.find(key);
                if (note_it != table.note_map.end()) {
                    inst_index = note_it.value();
                } else {
                    for (auto it = table.note_map.begin(); it != table.note_map.end(); ++it) {
                        if (it.key() <= key) {
                            inst_index = it.value();
                        }
                    }
                }
            }
        }

        const VoiceGroup &split_vg = vg_it.value();
        if (inst_index < 0 || inst_index >= split_vg.instruments.size()) return nullptr;
        return resolveInstrumentForEvent(&split_vg.instruments[inst_index], key, all_voicegroups, keysplit_tables);
    }

    if (inst->type_id == 0x80) { // voice_keysplit_all (drumset)
        auto it = all_voicegroups->find(inst->sample_label);
        if (it == all_voicegroups->end()) return nullptr;

        const VoiceGroup &drum_vg = it.value();
        const int drum_index = key - drum_vg.offset;
        if (drum_index < 0 || drum_index >= drum_vg.instruments.size()) return nullptr;
        return resolveInstrumentForEvent(&drum_vg.instruments[drum_index], key, all_voicegroups, keysplit_tables);
    }

    return inst;
}

QString playbackTypeAbbrev(const Instrument *inst) {
    if (!inst) return "-";

    static const QMap<int, QString> baseType = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> dutyMap = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    const int type_id = inst->type_id;
    if (baseType.contains(type_id)) {
        return baseType.value(type_id);
    }

    const QString duty = dutyMap.value(inst->duty_cycle & 0x03, "50");

    switch (type_id) {
    case 0x01: // Square1 (sweep)
    case 0x09:
        return QString("Sq.%1S").arg(duty);
    case 0x02: // Square2
    case 0x0A:
        return QString("Sq.%1").arg(duty);
    case 0x04: // Noise
    case 0x0C:
        return (inst->duty_cycle & 0x1) ? "Ns.7" : "Ns.15";
    default:
        break;
    }

    return "-";
}
} // namespace


/// controller loads the song, creates the track items, etc.
Controller::Controller(MainWindow *window) : QObject(window) {
    this->m_window = window->ui;
    this->m_piano_roll = new PianoRoll(this);
    connect(this->m_piano_roll, &PianoRoll::eventItemSelected, this, &Controller::displayEvent);
    this->m_track_roll = new TrackRoll(this);
    connect(this->m_track_roll, &TrackRoll::trackMuteToggled, this, &Controller::onTrackMuteToggled);
    connect(this->m_track_roll, &TrackRoll::trackSoloToggled, this, &Controller::onTrackSoloToggled);
    connect(this->m_track_roll, &TrackRoll::trackSelected, this, &Controller::onTrackSelected);
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

    m_window->view_trackRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_trackRoll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_window->view_measures_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures_tracks->setFrameShape(QFrame::NoFrame);

    m_window->view_measures->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_window->view_measures->setFrameShape(QFrame::NoFrame);

    if (m_window->scrollArea) {
        m_window->scrollArea->setFrameShape(QFrame::NoFrame);
        m_window->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_window->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    if (m_window->scroll_pianoArea) {
        m_window->scroll_pianoArea->setFrameShape(QFrame::NoFrame);
        m_window->scroll_pianoArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_window->scroll_pianoArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

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

    // Navigation: center vertically on midpoint of *song's* note range
    if (m_window->view_piano && m_window->view_pianoRoll) {
        int min_key = 127;
        int max_key = 0;
        bool has_notes = false;
        QVector<int> keys;
        if (m_song) {
            const auto &notes = m_song->getNotes();
            for (const auto &pair : notes) {
                if (m_song->isMetaTrack(pair.first)) continue;
                if (!pair.second) continue;
                const int key = pair.second->getKeyNumber();
                if (key < min_key) min_key = key;
                if (key > max_key) max_key = key;
                keys.append(key);
                has_notes = true;
            }
        }

        QRectF bounds = m_piano_roll->scenePiano()->itemsBoundingRect();
        if (bounds.isValid()) {
            qreal target_y = bounds.center().y();
            if (has_notes) {
                int use_min = min_key;
                int use_max = max_key;
                if (keys.size() >= 8) {
                    std::sort(keys.begin(), keys.end());
                    const int n = keys.size();
                    const int low_idx = std::clamp(static_cast<int>(n * 0.05), 0, n - 1);
                    const int high_idx = std::clamp(static_cast<int>(n * 0.95), 0, n - 1);
                    use_min = keys[low_idx];
                    use_max = keys[high_idx];
                }
                const qreal min_y = scoreNotePosition(use_min).y;
                const qreal max_y = scoreNotePosition(use_max).y;
                target_y = (min_y + max_y) * 0.5;
            }
            const QPointF piano_center = m_window->view_piano->mapToScene(m_window->view_piano->viewport()->rect().center());
            const QPointF roll_center = m_window->view_pianoRoll->mapToScene(m_window->view_pianoRoll->viewport()->rect().center());
            m_window->view_piano->centerOn(piano_center.x(), target_y);
            m_window->view_pianoRoll->centerOn(roll_center.x(), target_y);
        }
    }

    // LCD timer removed; time display is in meta box.
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
    if (title.isEmpty()) {
        return;
    }

    if (m_window && m_window->listView_songTable && m_window->listView_songTable->model()) {
        auto *view = m_window->listView_songTable;
        auto *model = view->model();
        bool matched = false;
        const int rows = model->rowCount();
        for (int row = 0; row < rows; ++row) {
            QModelIndex idx = model->index(row, 0);
            if (!idx.isValid()) continue;
            if (idx.data(Qt::UserRole).toString() == title) {
                view->setCurrentIndex(idx);
                m_current_song_index = row;
                matched = true;
                break;
            }
        }
        if (!matched) {
            m_current_song_index = index.row();
        }
    } else {
        m_current_song_index = index.row();
    }

    emit songSelected(title);

    SongEntry &entry = this->m_project->getSongEntryByTitle(title);

    if (entry.midifile.isEmpty()) {
        QMessageBox::warning(nullptr, "Missing MIDI", QString("No MIDI file defined for %1.").arg(title));
        return;
    }

    // std::shared_ptr<Song> song = this->m_project->getSong(title);
    // !TODO: load unloaded songs here (well, in the project function)

    qDebug() << "Clicked" << title;
    qDebug() << "voicegroup:" << entry.voicegroup << "player:" << entry.player << "file:" << entry.midifile;

    //this->m_project->addSong(title);

    if (!this->m_project->songLoaded(title)) {
        smf::MidiFile midi = this->m_interface->loadMidi(title);
        if (midi.getTrackCount() == 0 || (midi.getTrackCount() > 0 && midi[0].size() == 0)) {
            QMessageBox::warning(nullptr, "Missing MIDI", QString("MIDI file for %1 could not be loaded.").arg(title));
            entry.midifile.clear();
            if (m_song_list_model) {
                QModelIndex idx = m_song_list_model->index(index.row(), 0);
                emit m_song_list_model->dataChanged(idx, idx);
            }
            return;
        }
        this->m_project->addSong(title, midi);
    }

    //std::shared_ptr<Song> song = this->m_project->getSong(title);
    this->m_project->setActiveSong(title);

    if (this->loadSong()) {
        if (auto *main_window = qobject_cast<MainWindow *>(parent())) {
            main_window->setWindowTitle(QString("gtm - %1").arg(title));
        }
    }

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
    if (m_window->label_MetaTimeValue) {
        m_window->label_MetaTimeValue->setText(time_text);
    }

    int current_tick = this->m_song->getTickFromTime(current_seconds);

    if (m_player_timer.isActive()) {
        if (m_last_play_tick >= 0 && current_tick < m_last_play_tick) {
            // Loop or backward jump: force autoscroll to resume.
            m_autoscroll_enabled = true;
            m_scroll_pos_valid = false;
            m_scroll_debounce.stop();
            m_force_scroll_to_playhead = true;
        }
        m_last_play_tick = current_tick;
    } else {
        m_last_play_tick = current_tick;
    }

    this->m_measure_roll->updatePlaybackGuide(current_tick);
    this->m_measure_roll->setTick(current_tick);
    this->updateSongPositionDisplay(current_tick);

    // Update track items with current instrument info
    Mixer *mixer = m_player->getMixer();

    int playhead_x = current_tick * ui_tick_x_scale;
    int viewport_left = this->m_window->hscroll_pianoRoll->value();
    int viewport_width = this->m_window->view_measures->viewport()->width();
    int activation_point = viewport_left + viewport_width / 4;

    if (!m_autoscroll_enabled && !m_scroll_debounce.isActive() && playhead_x >= activation_point) {
        m_autoscroll_enabled = true;
    }

    if (m_autoscroll_enabled && !m_autoscroll_prev) {
        m_scroll_pos_valid = false;
    }
    m_autoscroll_prev = m_autoscroll_enabled;

    if (m_autoscroll_enabled) {
        m_scroll_frame = (m_scroll_frame + 1) & 1;
        if (m_scroll_frame != 0) {
            // 30 Hz autoscroll
            return;
        }
        int target = playhead_x - viewport_width / 4;
        if (target < 0) target = 0;
        int current = this->m_window->hscroll_pianoRoll->value();
        if (m_force_scroll_to_playhead) {
            m_force_scroll_to_playhead = false;
            m_scroll_pos = static_cast<double>(target);
            m_scroll_pos_valid = true;
            if (current != target) {
                this->m_window->hscroll_pianoRoll->setValue(target);
            }
        } else if (target < current) {
            target = current; // never scroll backwards during autoscroll
        }
        if (!m_scroll_pos_valid) {
            m_scroll_pos = static_cast<double>(m_force_scroll_to_playhead ? target : current);
            m_scroll_pos_valid = true;
        }
        const double alpha = 0.75;
        m_scroll_pos = m_scroll_pos + (static_cast<double>(target) - m_scroll_pos) * alpha;
        int scroll_value = static_cast<int>(std::llround(m_scroll_pos));
        if (current != scroll_value) {
            this->m_window->hscroll_pianoRoll->setValue(scroll_value);
        }
    }

    if (m_song_duration_ticks > 0 && current_tick >= m_song_duration_ticks) {
        this->stop();
        if (m_window->Button_Play) m_window->Button_Play->setChecked(false);
        if (m_window->Button_Pause) m_window->Button_Pause->setChecked(false);
        if (m_window->Button_Stop) m_window->Button_Stop->setChecked(true);
        if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(true);
        m_edits_enabled = true;
        if (m_piano_roll) m_piano_roll->setEditsEnabled(true);
    }

    // Update meters at UI tick rate
    if (mixer) {
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
    m_song_duration_ticks = m_song ? m_song->durationInTicks() : 0;

    this->m_track_roll->setSong(song);
    this->m_piano_roll->setSong(song);
    this->m_measure_roll->setSong(song);

    this->displayRolls();
    this->updateSongMetaDisplay();

    // reset playhead to start
    this->m_measure_roll->setTick(0);
    this->m_measure_roll->updatePlaybackGuide(0);
    if (m_window && m_window->hscroll_pianoRoll) {
        m_window->hscroll_pianoRoll->setValue(0);
    }
    if (m_window->label_MetaTimeValue) {
        m_window->label_MetaTimeValue->setText("00:00.000");
    }
    // Clear solo/mute UI state between songs.
    if (m_track_roll) {
        m_track_roll->clearAllSoloed();
        for (int ch = 0; ch < g_num_midi_channels; ++ch) {
            m_track_roll->setTrackMuted(ch, false);
        }
    }
    if (m_player) {
        m_player->getMixer()->setAllMuted(false);
    }
    if (m_window->Button_Play) m_window->Button_Play->setChecked(false);
    if (m_window->Button_Pause) m_window->Button_Pause->setChecked(false);
    if (m_window->Button_Stop) m_window->Button_Stop->setChecked(true);

    return true;
}

static QString keySignatureToString(int sharps_flats, int is_minor) {
    static const QString major_keys[15] = {
        "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C",
        "G", "D", "A", "E", "B", "F#", "C#"
    };
    static const QString minor_keys[15] = {
        "Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am",
        "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "A#m"
    };

    int idx = sharps_flats + 7;
    if (idx < 0 || idx >= 15) return "C";
    return is_minor ? minor_keys[idx] : major_keys[idx];
}

void Controller::updateSongMetaDisplay() {
    if (!m_window || !m_song) return;

    QString tempo_text = "120.0 BPM";
    QString time_sig_text = "4/4";
    QString key_text = "C";

    const SongEntry &meta = m_song->getMetaInfo();
    if (m_window->label_SongInfoSongIdValue) {
        m_window->label_SongInfoSongIdValue->setText(meta.title);
    }
    if (m_window->label_SongInfoPlayerTypeValue) {
        if (m_window->label_SongInfoPlayerTypeValue->findText(meta.player) < 0) {
            m_window->label_SongInfoPlayerTypeValue->clear();
            m_window->label_SongInfoPlayerTypeValue->addItem(meta.player);
        }
        m_window->label_SongInfoPlayerTypeValue->setCurrentText(meta.player);
    }
    if (m_window->label_SongInfoPriorityValue) {
        m_window->label_SongInfoPriorityValue->setText(QString::number(meta.priority));
    }
    if (m_window->label_SongInfoReverbValue) {
        m_window->label_SongInfoReverbValue->setValue(meta.reverb);
    }
    if (m_window->label_SongInfoVolumeValue) {
        m_window->label_SongInfoVolumeValue->setValue(meta.volume);
    }
    if (m_window->label_SongInfoTPQNValue) {
        m_window->label_SongInfoTPQNValue->setText(QString::number(m_song->getTicksPerQuarterNote()));
    }
    if (m_window->label_SongInfoVoicegroupValue) {
        if (m_window->label_SongInfoVoicegroupValue->findText(meta.voicegroup) < 0) {
            m_window->label_SongInfoVoicegroupValue->clear();
            m_window->label_SongInfoVoicegroupValue->addItem(meta.voicegroup);
        }
        m_window->label_SongInfoVoicegroupValue->setCurrentText(meta.voicegroup);
    }
    if (m_window->label_SongInfoMusicTracksValue) {
        int tracks = 0;
        int total = m_song->getTrackCount();
        for (int i = 0; i < total; ++i) {
            if (!m_song->isMetaTrack(i)) tracks++;
        }
        m_window->label_SongInfoMusicTracksValue->setText(QString::number(tracks));
    }

    auto &tempos = m_song->getTempoChanges();
    if (!tempos.isEmpty()) {
        auto it = tempos.begin();
        smf::MidiEvent *e = it.value();
        if (e) {
            tempo_text = QString("%1 BPM").arg(e->getTempoBPM(), 0, 'f', 1);
        }
    }

    auto &sigs = m_song->getTimeSignatures();
    if (!sigs.isEmpty()) {
        auto it = sigs.begin();
        smf::MidiEvent *e = it.value();
        if (e) {
            int num = (*e)[3];
            int den = static_cast<int>(std::pow(2, (*e)[4]));
            time_sig_text = QString("%1/%2").arg(num).arg(den);
        }
    }

    auto &keys = m_song->getKeySignatures();
    if (!keys.isEmpty()) {
        auto it = keys.begin();
        smf::MidiEvent *e = it.value();
        if (e) {
            int sharps_flats = static_cast<int>(static_cast<int8_t>((*e)[3]));
            int is_minor = (*e)[4];
            key_text = keySignatureToString(sharps_flats, is_minor);
        }
    }

    if (m_window->label_MetaTempoValue) {
        m_window->label_MetaTempoValue->setText(tempo_text);
    }
    if (m_window->label_MetaTimeSigValue) {
        m_window->label_MetaTimeSigValue->setText(time_sig_text);
    }
    if (m_window->label_MetaTickValue) {
        m_window->label_MetaTickValue->setText("0");
    }
    if (m_window->label_MetaTimeValue) {
        m_window->label_MetaTimeValue->setText("00:00.000");
    }
    if (m_window->label_MetaMeasureBeatValue) {
        m_window->label_MetaMeasureBeatValue->setText("1.1");
    }
}

void Controller::updateSongPositionDisplay(int tick) {
    if (!m_window || !m_song) return;
    if (tick == m_last_meta_tick) return;
    m_last_meta_tick = tick;

    int end_tick = m_song_duration_ticks;
    if (end_tick <= 0) {
        if (m_window->label_MetaTickValue) {
            m_window->label_MetaTickValue->setText(QString::number(0));
        }
        if (m_window->label_MetaMeasureBeatValue) {
            m_window->label_MetaMeasureBeatValue->setText("1.1");
        }
        return;
    }
    if (tick >= end_tick) {
        tick = end_tick - 1;
        if (tick < 0) tick = 0;
    }

    int measure_number = 1;
    int beat_number = 1;

    int tpqn = m_song->getTicksPerQuarterNote();
    if (tpqn > 0) {
        auto &time_sigs = m_song->getTimeSignatures();
        int current_num = 4;
        int current_den = 4;
        auto it_sig = time_sigs.begin();
        auto end_sig = time_sigs.end();

        if (it_sig != end_sig && it_sig.key() == 0) {
            smf::MidiEvent *first_sig = it_sig.value();
            current_num = (*first_sig)[3];
            current_den = static_cast<int>(std::pow(2, (*first_sig)[4]));
        }

        int current_tick = 0;
        while (current_tick <= tick) {
            if (it_sig != end_sig && it_sig.key() <= current_tick) {
                smf::MidiEvent *sig_event = it_sig.value();
                current_num = (*sig_event)[3];
                current_den = static_cast<int>(std::pow(2, (*sig_event)[4]));
            }

            int ticks_per_beat = (tpqn * 4 / current_den);
            int ticks_per_measure = current_num * ticks_per_beat;

            int end_of_segment = m_song_duration_ticks;
            if (it_sig != end_sig) {
                auto next_it = it_sig;
                ++next_it;
                if (next_it != end_sig) {
                    end_of_segment = next_it.key();
                }
            }

            while (current_tick < end_of_segment) {
                int measure_start = current_tick;
                int measure_end = std::min(current_tick + ticks_per_measure, end_of_segment);

                if (tick < measure_end) {
                    int offset = tick - measure_start;
                    beat_number = (offset / ticks_per_beat) + 1;
                    if (beat_number < 1) beat_number = 1;
                    if (beat_number > current_num) beat_number = current_num;
                    current_tick = tick + 1;
                    break;
                }

                measure_number++;
                current_tick = measure_end;
            }

            if (current_tick > tick) break;
            if (it_sig != end_sig) it_sig++;
        }
    }

    if (m_window->label_MetaTickValue) {
        m_window->label_MetaTickValue->setText(QString::number(tick));
    }
    if (m_window->label_MetaMeasureBeatValue) {
        m_window->label_MetaMeasureBeatValue->setText(QString("%1.%2").arg(measure_number).arg(beat_number));
    }
}

void Controller::displayEvent(smf::MidiEvent *event) {
    // TODO: safety checks (isNote / isNoteOn, etc)
    if (!event) {
        // error
        return;
    }

    const int max_tick = (m_song_duration_ticks > 0) ? m_song_duration_ticks : std::numeric_limits<int>::max();
    m_window->spinBox_NoteOnTick->setMaximum(max_tick);
    m_window->spinBox_NoteOffTick->setMaximum(max_tick);
    m_window->spinBox_NoteChannel->setValue(event->getChannel());
    m_window->spinBox_NoteKey->setValue(event->getKeyNumber());
    m_window->spinBox_NoteOnTick->setValue(event->tick);
    m_window->spinBox_NoteOnVelocity->setValue(event->getVelocity());
    if (event->hasLink()) {
        m_window->spinBox_NoteOffTick->setValue(event->getLinkedEvent()->tick);
        m_window->spinBox_NoteOffVelocity->setValue(event->getLinkedEvent()->getVelocity());
    } else {
        m_window->spinBox_NoteOffTick->setValue(event->tick);
        m_window->spinBox_NoteOffVelocity->setValue(0);
    }

    if (m_window->lineEdit_EventTrackValue) {
        int display_row = m_song ? m_song->getDisplayRow(event->track) : -1;
        m_window->lineEdit_EventTrackValue->setText(display_row >= 0 ? QString::number(display_row) : "-");
    }

    uint8_t program = programForEvent(m_song.get(), event);
    if (m_window->lineEdit_EventProgramValue) {
        m_window->lineEdit_EventProgramValue->setText(QString::number(program));
    }

    const Instrument *resolved = nullptr;
    if (m_song && m_project) {
        const VoiceGroup *voicegroup = m_project->getVoiceGroup(m_song->getMetaInfo().voicegroup);
        const QMap<QString, VoiceGroup> *all_voicegroups = &m_project->getVoiceGroups();
        const QMap<QString, KeysplitTable> *keysplit_tables = &m_project->getKeysplitTables();
        if (voicegroup && program < voicegroup->instruments.size()) {
            const Instrument *base = &voicegroup->instruments[program];
            resolved = resolveInstrumentForEvent(base,
                                                 static_cast<uint8_t>(event->getKeyNumber()),
                                                 all_voicegroups,
                                                 keysplit_tables);
        }
    }

    if (m_window->lineEdit_EventTypeValue) {
        m_window->lineEdit_EventTypeValue->setText(playbackTypeAbbrev(resolved));
    }
    if (m_window->lineEdit_EventAdsrValue) {
        if (resolved) {
            m_window->lineEdit_EventAdsrValue->setText(
                QString("%1/%2/%3/%4")
                    .arg(resolved->attack)
                    .arg(resolved->decay)
                    .arg(resolved->sustain)
                    .arg(resolved->release));
        } else {
            m_window->lineEdit_EventAdsrValue->setText("-");
        }
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
    m_scroll_pos_valid = false;
    m_last_play_tick = -1;
    m_force_scroll_to_playhead = true;
    m_edits_enabled = false;
    m_player->play();

    m_player_elapsed.start();
    m_player_timer.start(16);
    if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(false);
    if (m_piano_roll) m_piano_roll->setEditsEnabled(false);
}

void Controller::stop() {
    m_player->stop();
    m_player_timer.stop();
    m_last_play_tick = -1;
    m_force_scroll_to_playhead = false;
    m_edits_enabled = true;
    m_track_roll->clearAllPlayingInfo();
    m_track_roll->clearAllMeters();
    if (m_master_meter) {
        m_master_meter->setLevels(0.0f, 0.0f);
    }
    if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(true);
    if (m_piano_roll) m_piano_roll->setEditsEnabled(true);
}

void Controller::pause() {
    m_player->stop();
    m_player_timer.stop();
    m_last_play_tick = -1;
    m_force_scroll_to_playhead = false;
    m_edits_enabled = true;
    m_track_roll->clearAllPlayingInfo();
    m_track_roll->clearAllMeters();
    if (m_master_meter) {
        m_master_meter->setLevels(0.0f, 0.0f);
    }
    if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(true);
    if (m_piano_roll) m_piano_roll->setEditsEnabled(true);
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

void Controller::onTrackSelected(int track) {
    if (m_piano_roll) {
        m_piano_roll->selectNotesForTrack(track, true);
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
    m_scroll_pos_valid = false;
    m_last_play_tick = tick;
    m_force_scroll_to_playhead = true;
}

void Controller::seekToStart() {
    seekToTick(0);
}

int Controller::currentTick() const {
    if (!m_song) return 0;
    // When not playing, the sequencer time can be stale; use the UI playhead instead.
    if (!m_player_timer.isActive()) {
        return m_measure_roll ? m_measure_roll->tick() : 0;
    }
    double current_seconds = m_player->getSequencer()->getCurrentTime();
    return m_song->getTickFromTime(current_seconds);
}

int Controller::currentSongIndex() const {
    return m_current_song_index;
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
    m_current_song_index = index;
    songListSongRequested(next);
    return true;
}

bool Controller::selectSongByTitle(const QString &title) {
    if (!m_window || !m_window->listView_songTable) return false;
    if (title.isEmpty()) return false;
    auto *view = m_window->listView_songTable;
    auto *model = view->model();
    if (!model) return false;
    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        QModelIndex idx = model->index(row, 0);
        if (!idx.isValid()) continue;
        const QString item_title = idx.data(Qt::UserRole).toString();
        if (item_title == title && (model->flags(idx) & Qt::ItemIsEnabled)) {
            view->setCurrentIndex(idx);
            m_current_song_index = row;
            songListSongRequested(idx);
            return true;
        }
    }
    return false;
}

bool Controller::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->angleDelta().x() != 0) {
            m_autoscroll_enabled = false;
            m_scroll_debounce.start();
            m_scroll_pos_valid = false;
        }
    }

    if (watched == m_window->view_measures->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);

        if (mouse_event->button() == Qt::LeftButton && m_song) {
            QPointF scene_pos = m_window->view_measures->mapToScene(mouse_event->pos());

            int tick = static_cast<int>(scene_pos.x() / ui_tick_x_scale);
            int tpqn = m_song->getTicksPerQuarterNote();
            int snapped_tick = qRound(static_cast<double>(tick) / tpqn) * tpqn;
            int end_tick = m_song_duration_ticks;
            if (end_tick <= 0 && m_song) end_tick = m_song->durationInTicks();
            snapped_tick = qBound(0, snapped_tick, end_tick);

            seekToTick(snapped_tick);
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}
