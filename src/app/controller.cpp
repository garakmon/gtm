#include "app/controller.h"

#include "app/project.h"
#include "app/projectinterface.h"
#include "app/songeditor.h"
#include "deps/midifile/MidiEvent.h"
#include "sound/song.h"
#include "ui/colors.h"
#include "ui/mainwindow.h"
#include "ui/measureroll.h"
#include "ui/meters.h"
#include "ui/midispinboxes.h"
#include "ui/minimapwidget.h"
#include "ui/pianoroll.h"
#include "ui/songlistmodel.h"
#include "ui/trackroll.h"
#include "ui_mainwindow.h"
#include "util/constants.h"
#include "util/logging.h"
#include "util/util.h"

#include <QFrame>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSet>
#include <QTime>
#include <QToolButton>
#include <QVector>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>



/**
 * Local utility functions that are so specific to this class, that they are better off in
 * this anonymous namespace than in the util module.
 */
namespace {
struct KeySignatureState {
    int sharps_flats = 0;
    bool is_minor = false;
    bool has_meta_signature = false;
};

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
        return resolveInstrumentForEvent(&split_vg.instruments[inst_index], key,
                                         all_voicegroups, keysplit_tables);
    }

    if (inst->type_id == 0x80) { // voice_keysplit_all (drumset)
        auto it = all_voicegroups->find(inst->sample_label);
        if (it == all_voicegroups->end()) return nullptr;

        const VoiceGroup &drum_vg = it.value();
        const int drum_index = key - drum_vg.offset;
        if (drum_index < 0 || drum_index >= drum_vg.instruments.size()) return nullptr;
        return resolveInstrumentForEvent(&drum_vg.instruments[drum_index], key,
                                         all_voicegroups, keysplit_tables);
    }

    return inst;
}

QString playbackTypeAbbrev(const Instrument *inst) {
    if (!inst) return "-";

    static const QMap<int, QString> s_base_type = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> s_duty_map = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    const int type_id = inst->type_id;
    if (s_base_type.contains(type_id)) {
        return s_base_type.value(type_id);
    }

    const QString duty = s_duty_map.value(inst->duty_cycle & 0x03, "50");

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

QPair<int, bool> keySignatureAtTick(const Song *song, int tick) {
    if (!song) return {0, false};
    const auto &keys = song->getKeySignatures();
    if (keys.isEmpty()) return {0, false};

    auto it = keys.upperBound(tick);
    if (it == keys.begin()) return {0, false};
    --it;
    smf::MidiEvent *ev = it.value();
    if (!ev || ev->size() < 5) return {0, false};
    return {static_cast<int8_t>((*ev)[3]), ((*ev)[4] != 0)};
}

/**
 * Snap a measure-roll click to a measure or beat boundary for the active
 * time-signature segment. Clicks in the top header snap down to the current
 * measure start, while lower clicks snap to the nearest beat in that segment.
 */
static int snapMeasureViewTick(const Song *song, int tick, qreal scene_y) {
    if (!song) {
        return tick;
    }

    const int tpqn = song->getTicksPerQuarterNote();
    if (tpqn <= 0) {
        return tick;
    }

    const QMap<int, smf::MidiEvent *> &time_sigs = song->getTimeSignatures();
    int current_num = 4;
    int current_den = 4;

    auto it_sig = time_sigs.upperBound(tick);
    if (it_sig != time_sigs.begin()) {
        it_sig--;
        smf::MidiEvent *sig_event = it_sig.value();
        if (sig_event && sig_event->size() >= 5) {
            current_num = (*sig_event)[3];
            current_den = static_cast<int>(std::pow(2, (*sig_event)[4]));
        }
    }

    const int ticks_per_beat = tpqn * 4 / current_den;
    const int ticks_per_measure = current_num * ticks_per_beat;
    const int segment_start_tick = (it_sig != time_sigs.end()) ? it_sig.key() : 0;

    if (scene_y < 10.0) {
        const int measure_index = (tick - segment_start_tick) / ticks_per_measure;
        return segment_start_tick + (measure_index * ticks_per_measure);
    }

    const int beat_index =
        qRound(static_cast<double>(tick - segment_start_tick) / ticks_per_beat);
    return segment_start_tick + (beat_index * ticks_per_beat);
}

/**
 * Based on the Krumhansl-Schmuckler algorithm for key-finding, which compares the distribution of
 * pitch classes to key profiles in s_ks_minor_profile and s_ks_minor_profile. Finds a best matching
 * profile, prefering the one with fewer sharps/flats in the case of a tie.
 */
KeySignatureState inferKeySigFromContext(const Song *song, int tick, int focus_track = -1) {
    KeySignatureState out;
    if (!song) return out;

    const int tpqn = song->getTicksPerQuarterNote();
    const int window = (tpqn > 0) ? (tpqn * 4) : 480;
    const int tick_min = tick - window;
    const int tick_max = tick + window;

    QVector<double> histogram(12, 0.0);
    bool has_notes = false;

    const auto &notes = song->getNotes();
    for (const auto &pair : notes) {
        const int track = pair.first;
        if (song->isMetaTrack(track)) continue;
        smf::MidiEvent *ev = pair.second;
        if (!ev || !ev->isNoteOn()) continue;
        if (ev->tick < tick_min || ev->tick > tick_max) continue;

        int pc = ev->getKeyNumber() % 12;
        if (pc < 0) pc += 12;

        const double weight = (track == focus_track) ? 2.0 : 1.0;
        histogram[pc] += weight;
        has_notes = true;
    }

    if (!has_notes) {
        return out;
    }

    struct Candidate {
        int sf;
        bool is_minor;
    };

    // https://extras.humdrum.org/man/keycor/
    static const QVector<double> s_ks_major_profile = {
        6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
    };

    static const QVector<double> s_ks_minor_profile = {
        6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
    };

    static const QVector<Candidate> s_candidates = {
        {-7, false}, {-6, false}, {-5, false}, {-4, false}, {-3, false}, {-2, false}, {-1, false},
        {0, false},
        {1, false}, {2, false}, {3, false}, {4, false}, {5, false}, {6, false}, {7, false},
        {-7, true}, {-6, true}, {-5, true}, {-4, true}, {-3, true}, {-2, true}, {-1, true},
        {0, true},
        {1, true}, {2, true}, {3, true}, {4, true}, {5, true}, {6, true}, {7, true}
    };

    auto majorTonicFromSf = [](int sf) {
        // sf to major tonic
        int tonic = 0;
        if (sf > 0) {
            tonic = (sf * 7) % 12;
        } else if (sf < 0) {
            tonic = ((-sf) * 5) % 12;
        }
        return tonic;
    };

    auto correlation = [](const QVector<double> &hist, const QVector<double> &profile, int tonic) {
        // normalized correlation
        double sum_x = 0.0;
        double sum_y = 0.0;
        for (int i = 0; i < 12; ++i) {
            sum_x += hist[i];
            sum_y += profile[(i - tonic + 12) % 12];
        }
        const double mean_x = sum_x / 12.0;
        const double mean_y = sum_y / 12.0;

        double num = 0.0;
        double den_x = 0.0;
        double den_y = 0.0;
        for (int i = 0; i < 12; ++i) {
            const double x = hist[i] - mean_x;
            const double y = profile[(i - tonic + 12) % 12] - mean_y;
            num += x * y;
            den_x += x * x;
            den_y += y * y;
        }
        const double den = std::sqrt(den_x * den_y);
        if (den <= 1e-12) return -1.0;
        return num / den;
    };

    double best_score = -2.0;
    int best_sf = 0;
    bool best_minor = false;

    auto scoreCandidate = [&](const Candidate &cand) {
        const int major_tonic = majorTonicFromSf(cand.sf);
        // relative minor shift
        const int tonic = cand.is_minor ? ((major_tonic + 9) % 12) : major_tonic;
        const QVector<double> &profile = cand.is_minor ? s_ks_minor_profile : s_ks_major_profile;
        return correlation(histogram, profile, tonic);
    };

    auto updateBest = [&](const Candidate &cand) {
        const double score = scoreCandidate(cand);
        if (score > best_score) {
            best_score = score;
            best_sf = cand.sf;
            best_minor = cand.is_minor;
            return;
        }
        if (std::abs(score - best_score) < 1e-9) {
            if (std::abs(cand.sf) < std::abs(best_sf)) {
                best_sf = cand.sf;
                best_minor = cand.is_minor;
            }
        }
    };

    for (const auto &cand : s_candidates) updateBest(cand);

    out.sharps_flats = best_sf;
    out.is_minor = best_minor;
    out.has_meta_signature = false;
    return out;
}

QString keySignatureToString(int sharps_flats, int is_minor) {
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
} // namespace



Controller::Controller(MainWindow *window) : QObject(window) {
    m_window = window->ui;
    m_piano_roll = new PianoRoll(this);
    m_track_roll = new TrackRoll(this);
    m_measure_roll = new MeasureRoll(this);

    m_player = std::make_unique<Player>();
    if (!m_player->initializeAudio()) {
        logging::error("Failed to initialize audio.", logging::LogCategory::Audio);
    }

    this->connectSignals();
    this->setupRolls();
    if (window) {
        this->setMinimap(window->m_minimap);
    }
}

/**
 * Load the decomp project at the filepath, which points to the root path (eg pokeemerald)
 */
bool Controller::loadProject(const QString &root) {
    if (!m_project) {
        m_project = std::make_unique<Project>();
        m_interface = std::make_unique<ProjectInterface>(m_project.get());
        m_song_editor = std::make_unique<SongEditor>(m_project.get(), this);
        connect(m_song_editor.get(), &SongEditor::songNeedsRedrawing,
                this, &Controller::onSongNeedsRedrawing, Qt::UniqueConnection);
    } else if (!m_song_editor) {
        m_song_editor = std::make_unique<SongEditor>(m_project.get(), this);
        connect(m_song_editor.get(), &SongEditor::songNeedsRedrawing,
                this, &Controller::onSongNeedsRedrawing, Qt::UniqueConnection);
    }

    if (m_interface->loadProject(root)) {
        if (m_project) {
            m_project->setRootPath(root);
        }
        // display the project values in the ui
        this->displayProject();
        return true;
    }
    return false;
}

const QString &Controller::projectRoot() const {
    static const QString s_empty;
    return m_project ? m_project->rootPath() : s_empty;
}

bool Controller::createSong(const NewSongSettings &settings, QString *error) {
    if (!m_project) {
        if (error) *error = "No project is loaded.";
        return false;
    }
    if (!m_project->createSong(settings, error)) {
        return false;
    }
    this->displayProject();
    this->selectSongByTitle(settings.title.trimmed());
    return true;
}

bool Controller::hasUnsavedChanges() const {
    return m_project ? m_project->hasUnsavedChanges() : false;
}

QStringList Controller::voicegroupNames() const {
    if (!m_project) return {};

    QStringList names = m_project->getVoiceGroups().keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

const QMap<QString, VoiceGroup> &Controller::voicegroups() const {
    static const QMap<QString, VoiceGroup> s_empty_voicegroups;
    return m_project ? m_project->getVoiceGroups() : s_empty_voicegroups;
}

const QMap<QString, KeysplitTable> &Controller::keysplitTables() const {
    static const QMap<QString, KeysplitTable> s_empty_tables;
    return m_project ? m_project->getKeysplitTables() : s_empty_tables;
}

QStringList Controller::playerNames() const {
    if (!m_project) return {"MUS_PLAYER"};

    QSet<QString> players;
    const int song_count = m_project->getNumSongsInTable();
    for (int i = 0; i < song_count; ++i) {
        const QString title = m_project->getSongTitleAt(i);
        SongEntry &entry = m_project->getSongEntryByTitle(title);
        if (!entry.player.isEmpty()) {
            players.insert(entry.player);
        }
    }

    if (players.isEmpty()) {
        players.insert("MUS_PLAYER");
    }

    QStringList list = players.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QStringList Controller::songTitles() const {
    if (!m_project) return {};

    QStringList titles;
    const int song_count = m_project->getNumSongsInTable();
    titles.reserve(song_count);
    for (int i = 0; i < song_count; ++i) {
        titles.append(m_project->getSongTitleAt(i));
    }
    return titles;
}

SongEditor *Controller::songEditor() const {
    return m_song_editor.get();
}

bool Controller::canUndoHistory() const {
    return m_song_editor ? m_song_editor->canUndoHistory() : false;
}

bool Controller::canRedoHistory() const {
    return m_song_editor ? m_song_editor->canRedoHistory() : false;
}

void Controller::undoHistory() {
    if (!m_song_editor) return;
    m_song_editor->undo();
}

void Controller::redoHistory() {
    if (!m_song_editor) return;
    m_song_editor->redo();
}

bool Controller::deleteSelectedEvents(QString *error) {
    if (!m_playback_state.edits_enabled || !m_song_editor) {
        return false;
    }

    return m_song_editor->deleteSelectedEvents(error);
}

/**
 * Auto load the project's active song.
 */
bool Controller::loadSong() {
    return this->loadSong(m_project->activeSong());
}

/**
 * Load a specific song.
 * 
 * Passes the song along to each of the rolls so they can display it.
 * Initializes the Player and Mixer for a new song.
 * Updates some ui elements outside the roll contexts.
 */
bool Controller::loadSong(std::shared_ptr<Song> song) {
    this->stop();

    song->load();

    m_song = song;
    m_playback_state.song_duration_ticks = m_song ? m_song->durationInTicks() : 0;
    this->rebuildInferredKeySignatureCache();

    if (m_track_roll && m_project && m_song) {
        const SongEntry &meta = m_song->getMetaInfo();
        const VoiceGroup *voicegroup = m_project->getVoiceGroup(meta.voicegroup);
        m_track_roll->setInstrumentContext(voicegroup, &m_project->getVoiceGroups(),
                                           &m_project->getKeysplitTables());
    }

    m_track_roll->setSong(song);
    m_piano_roll->setSong(song);
    m_piano_roll->setActiveTrack(-1);
    m_measure_roll->setSong(song);

    this->displayRolls();
    this->updateSongMetaDisplay();

    // reset playhead to start
    m_measure_roll->setTick(0);
    m_measure_roll->updatePlaybackGuide(0);
    if (m_window && m_window->hscroll_pianoRoll) {
        m_window->hscroll_pianoRoll->setValue(0);
    }
    if (m_window->label_MetaTimeValue) {
        m_window->label_MetaTimeValue->setText("00:00.000");
    }
    // clear solo/mute UI state between songs
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

/**
 * Performs one-time initial setup of the rolls and view policies.
 */
void Controller::setupRolls() {
    // scrolling should be synced -- accomplished by sharing a scroll bar between views
    m_window->view_piano->setVerticalScrollBar(m_window->vscroll_pianoRoll);
    m_window->view_pianoRoll->setVerticalScrollBar(m_window->vscroll_pianoRoll);

    m_window->view_trackList->setVerticalScrollBar(m_window->vscroll_trackRoll);
    m_window->view_trackRoll->setVerticalScrollBar(m_window->vscroll_trackRoll);

    // sync horizontal scrolling
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

    // stuff to encourage drawing efficiency
    m_window->view_piano->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_pianoRoll->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_trackList->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_trackRoll->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_measures->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_window->view_measures_tracks->setViewportUpdateMode(
        QGraphicsView::MinimalViewportUpdate);

    m_window->view_piano->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_pianoRoll->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_trackList->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_trackRoll->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_measures->setCacheMode(QGraphicsView::CacheBackground);
    m_window->view_measures_tracks->setCacheMode(QGraphicsView::CacheBackground);

    // event filters for auto scrolling overrides
    m_window->view_measures->viewport()->installEventFilter(this);
    m_window->view_pianoRoll->installEventFilter(this);
    m_window->view_pianoRoll->viewport()->installEventFilter(this);
    m_window->view_trackRoll->viewport()->installEventFilter(this);

    m_scroll_state.scroll_debounce.setSingleShot(true);
    m_scroll_state.scroll_debounce.setInterval(400);

    // hide scrollbars: minimap will handle horizontal, and mouse wheel can pan vertically.
    m_window->hscroll_pianoRoll->setVisible(false);
    m_window->vscroll_pianoRoll->setVisible(false);
    m_window->vscroll_trackRoll->setVisible(false);
}

/**
 * When a song is actually laoded, give that to the rolls so they can display.
 */
void Controller::displayRolls() {
    m_piano_roll->display();

    // view_piano: draws the piano keys
    m_window->view_piano->setScene(m_piano_roll->scenePiano());
    QRectF piano_bounds = m_piano_roll->scenePiano()->itemsBoundingRect();
    m_window->view_piano->setSceneRect(0.0, 0.0, piano_bounds.width(),
                                       piano_bounds.height());
    m_window->view_piano->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_piano->setRenderHint(QPainter::Antialiasing, false);
    m_window->view_piano->setBackgroundBrush(QBrush(QColor(0x282828), Qt::SolidPattern));

    // view_pianoRoll: where the score notes live
    m_window->view_pianoRoll->setScene(m_piano_roll->sceneRoll());
    QRectF roll_bounds = m_piano_roll->sceneRoll()->sceneRect();
    m_window->view_pianoRoll->setSceneRect(roll_bounds);
    m_window->view_pianoRoll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_window->view_pianoRoll->setBackgroundBrush(Qt::NoBrush);

    // view_trackRoll: needs to clip to number of tracks
    m_window->view_trackRoll->setScene(m_track_roll->sceneRoll());
    QRectF track_roll_bounds = m_track_roll->sceneRoll()->sceneRect();
    m_window->view_trackRoll->setSceneRect(track_roll_bounds);
    m_window->view_trackRoll->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // view_trackList
    m_window->view_trackList->setScene(m_track_roll->sceneTracks());
    QRectF track_list_bounds = m_track_roll->sceneTracks()->sceneRect();
    m_window->view_trackList->setSceneRect(track_list_bounds);
    m_window->view_trackList->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // view_measures_tracks & view_measures look the same,
    // except the track view shows the meta events that are clipped out for view_measures
    QRectF measure_bounds = m_measure_roll->sceneMeasures()->itemsBoundingRect();

    m_window->view_measures_tracks->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures_tracks->setSceneRect(0.0, 0.0, measure_bounds.width(),
                                                 ui_measure_roll_height);
    m_window->view_measures_tracks->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_window->view_measures->setScene(m_measure_roll->sceneMeasures());
    m_window->view_measures->setSceneRect(0.0, 0.0, measure_bounds.width(),
                                          ui_measure_info_height);
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
            const QPointF piano_center = m_window->view_piano->mapToScene(
                m_window->view_piano->viewport()->rect().center()
            );
            const QPointF roll_center = m_window->view_pianoRoll->mapToScene(
                m_window->view_pianoRoll->viewport()->rect().center()
            );
            m_window->view_piano->centerOn(piano_center.x(), target_y);
            m_window->view_pianoRoll->centerOn(roll_center.x(), target_y);
        }
    }
}

void Controller::redrawCurrentSong(bool rebuild_rolls) {
    if (!m_song) {
        return;
    }

    int piano_hscroll = 0;
    int piano_vscroll = 0;
    if (!rebuild_rolls && m_window) {
        // preserve scroll position during redraws so restoring selection
        // does not make the views jump to the edited note
        if (m_window->hscroll_pianoRoll) {
            piano_hscroll = m_window->hscroll_pianoRoll->value();
        }
        if (m_window->vscroll_pianoRoll) {
            piano_vscroll = m_window->vscroll_pianoRoll->value();
        }
    }

    if (rebuild_rolls) {
        m_track_roll->setSong(m_song);
    }
    m_piano_roll->setSong(m_song);
    m_measure_roll->setSong(m_song);
    this->displayRolls();
    if (m_song_editor) {
        m_piano_roll->selectEvents(m_song_editor->selectedEvents());
    }
    if (!rebuild_rolls && m_window) {
        if (m_window->hscroll_pianoRoll) {
            m_window->hscroll_pianoRoll->setValue(piano_hscroll);
        }
        if (m_window->vscroll_pianoRoll) {
            m_window->vscroll_pianoRoll->setValue(piano_vscroll);
        }
    }
    this->updateSongMetaDisplay();
}

/**
 * On project open, populate some project-related ui.
 */
void Controller::displayProject() {
    // display the song table list
    m_song_list_model = new SongListModel(m_project.get(), this);
    m_window->listView_songTable->setModel(m_song_list_model);

    // !TODO: comboboxes, voicegroup editor, etc.
}

/**
 * Catch user signals from the ui and process in Controller functions.
 */
void Controller::connectSignals() {
    // user clicks a note item in the piano roll
    connect(m_piano_roll, &PianoRoll::eventItemSelected,
            this, &Controller::displayEvent, Qt::UniqueConnection);
    connect(m_piano_roll, &PianoRoll::onSelectedEventsChanged,
            this, &Controller::onSelectedEventsChanged, Qt::UniqueConnection);
    connect(m_piano_roll, &PianoRoll::onNoteMoveRequested,
            this, &Controller::onNoteMoveRequested, Qt::UniqueConnection);
    connect(m_piano_roll, &PianoRoll::onNoteResizeRequested,
            this, &Controller::onNoteResizeRequested, Qt::UniqueConnection);
    connect(m_piano_roll, &PianoRoll::onNoteCreateRequested,
            this, &Controller::onNoteCreateRequested, Qt::UniqueConnection);
    connect(m_piano_roll, &PianoRoll::onNoteDeleteRequested,
            this, &Controller::onNoteDeleteRequested, Qt::UniqueConnection);

    // user clicks the mute button on a track widget in the track list
    connect(m_track_roll, &TrackRoll::trackMuteToggled,
            this, &Controller::onTrackMuteToggled, Qt::UniqueConnection);

    // user clicks the S button on a track widget
    connect(m_track_roll, &TrackRoll::trackSoloToggled,
            this, &Controller::onTrackSoloToggled, Qt::UniqueConnection);

    // user clicks anywhere outside the buttons on the track widget
    connect(m_track_roll, &TrackRoll::trackSelected,
            this, &Controller::onTrackSelected, Qt::UniqueConnection);
    connect(m_track_roll, &TrackRoll::trackRightClicked,
            this, &Controller::onTrackRightClicked, Qt::UniqueConnection);
    connect(m_track_roll, &TrackRoll::addTrackClicked,
            this, &Controller::onAddTrackClicked, Qt::UniqueConnection);
    connect(m_track_roll, &TrackRoll::deleteTrackClicked,
            this, &Controller::onDeleteTrackClicked, Qt::UniqueConnection);
    connect(m_measure_roll, &MeasureRoll::onTimeRangeSelected,
            this, &Controller::onTimeRangeSelected, Qt::UniqueConnection);
    connect(m_measure_roll, &MeasureRoll::onTimeSelectionCleared,
            this, &Controller::onTimeSelectionCleared, Qt::UniqueConnection);

    // signal comes from expanding/collapsing track widgets, or when tracks are redrawn
    connect(m_track_roll, &TrackRoll::layoutChanged,
            this, &Controller::onTrackLayoutChanged, Qt::UniqueConnection);

    // ~60Hz timer (16ms) to call updates on the ui while a song is playing
    connect(&m_player_timer, &QTimer::timeout,
            this, &Controller::syncRolls, Qt::UniqueConnection);

    // user double clicks a valid index in the song list
    if (m_window && m_window->listView_songTable) {
        connect(m_window->listView_songTable, &QListView::doubleClicked,
                this, &Controller::songListSongRequested, Qt::UniqueConnection);
    }

    if (m_window && m_window->button_note_add) {
        m_piano_roll->setCreateNotesEnabled(m_window->button_note_add->isChecked());
        connect(m_window->button_note_add, &QToolButton::toggled,
                m_piano_roll, &PianoRoll::setCreateNotesEnabled, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_select_rect) {
        m_piano_roll->setRectSelectEnabled(m_window->button_select_rect->isChecked());
        connect(m_window->button_select_rect, &QToolButton::toggled,
                m_piano_roll, &PianoRoll::setRectSelectEnabled, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_select_lasso) {
        m_piano_roll->setLassoSelectEnabled(m_window->button_select_lasso->isChecked());
        connect(m_window->button_select_lasso, &QToolButton::toggled,
                m_piano_roll, &PianoRoll::setLassoSelectEnabled, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_select_track) {
        m_piano_roll->setTrackSelectEnabled(m_window->button_select_track->isChecked());
        connect(m_window->button_select_track, &QToolButton::toggled,
                m_piano_roll, &PianoRoll::setTrackSelectEnabled, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_select_invert) {
        connect(m_window->button_select_invert, &QToolButton::clicked, m_piano_roll,
                &PianoRoll::invertSelection, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_select_time) {
        m_measure_roll->setTimeSelectEnabled(m_window->button_select_time->isChecked());
        connect(m_window->button_select_time, &QToolButton::toggled,
                m_measure_roll, &MeasureRoll::setTimeSelectEnabled, Qt::UniqueConnection);
    }
    if (m_window && m_window->button_note_delete) {
        m_piano_roll->setDeleteNotesEnabled(m_window->button_note_delete->isChecked());
        connect(m_window->button_note_delete, &QToolButton::toggled,
                m_piano_roll, &PianoRoll::setDeleteNotesEnabled, Qt::UniqueConnection);
    }
}

void Controller::onSelectedEventsChanged(const QVector<smf::MidiEvent *> &events) {
    if (!m_song_editor) {
        return;
    }

    m_song_editor->setSelectedEvents(events);
}

void Controller::onNoteMoveRequested(const NoteMoveSettings &settings) {
    if (!m_song_editor) {
        return;
    }

    QString error;
    if (!m_song_editor->moveSelectedNotes(settings, &error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onNoteResizeRequested(const NoteResizeSettings &settings) {
    if (!m_song_editor) {
        return;
    }

    QString error;
    if (!m_song_editor->resizeSelectedNotes(settings, &error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onNoteCreateRequested(const QVector<NoteCreateSettings> &notes) {
    if (!m_song_editor) {
        return;
    }

    QString error;
    if (!m_song_editor->createNotes(notes, &error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onNoteDeleteRequested(const QVector<smf::MidiEvent *> &events) {
    if (!m_song_editor) {
        return;
    }

    m_song_editor->setSelectedEvents(events);

    QString error;
    if (!m_song_editor->deleteSelectedEvents(&error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onSongNeedsRedrawing(bool rebuild_rolls) {
    this->redrawCurrentSong(rebuild_rolls);
}

/**
 * The function which controls the playback UI through ticks.
 * 
 * Update playhead and timer ui from the sequencer data, as well as Mixer visualization.
 * Coordinate the roll views and scrolling.
 * Detect song end and process accordingly.
 */
void Controller::syncRolls() {
    if (!m_song) return;

    // sample sequencer time
    double current_seconds = m_player->getSequencer()->getCurrentTime();

    // update time label
    QTime display_time(0, 0);
    display_time = display_time.addMSecs(static_cast<int>(current_seconds * 1000));
    QString time_text = display_time.toString("mm:ss.zzz");
    if (m_window->label_MetaTimeValue) {
        m_window->label_MetaTimeValue->setText(time_text);
    }

    // convert seconds to song tick
    int current_tick = m_song->getTickFromTime(current_seconds);

    // detect loop/backward jump
    if (m_player_timer.isActive()) {
        if (m_playback_state.last_play_tick >= 0
         && current_tick < m_playback_state.last_play_tick) {
            // force autoscroll recovery
            m_scroll_state.autoscroll_enabled = true;
            m_scroll_state.scroll_pos_valid = false;
            m_scroll_state.scroll_debounce.stop();
            m_playback_state.force_scroll_to_playhead = true;
        }
        m_playback_state.last_play_tick = current_tick;
    } else {
        m_playback_state.last_play_tick = current_tick;
    }

    // update playhead and song position ui
    m_measure_roll->updatePlaybackGuide(current_tick);
    m_measure_roll->setTick(current_tick);
    this->updateSongPositionDisplay(current_tick);

    // cache mixer for meter + voice updates
    Mixer *mixer = m_player->getMixer();

    // compute autoscroll trigger threshold
    int playhead_x = current_tick * ui_tick_x_scale;
    int viewport_left = m_window->hscroll_pianoRoll->value();
    int viewport_width = m_window->view_measures->viewport()->width();
    int activation_point = viewport_left + viewport_width / 4;

    // re-enable autoscroll after manual scroll
    if (!m_scroll_state.autoscroll_enabled
     && !m_scroll_state.scroll_debounce.isActive()
     && playhead_x >= activation_point) {
        m_scroll_state.autoscroll_enabled = true;
    }

    // reset interpolation on autoscroll resume
    if (m_scroll_state.autoscroll_enabled && !m_scroll_state.autoscroll_prev) {
        m_scroll_state.scroll_pos_valid = false;
    }
    m_scroll_state.autoscroll_prev = m_scroll_state.autoscroll_enabled;

    // drive horizontal autoscroll
    if (m_scroll_state.autoscroll_enabled) {
        m_scroll_state.scroll_frame = (m_scroll_state.scroll_frame + 1) & 1;
        if (m_scroll_state.scroll_frame != 0) {
            // 30 Hz autoscroll because it gets laggy at 60
            // !TODO: investigate^
            return;
        }
        // target playhead at quarter viewport width for autoscroll activation
        int target = playhead_x - viewport_width / 4;
        if (target < 0) {
            target = 0;
        }
        int current = m_window->hscroll_pianoRoll->value();
        if (m_playback_state.force_scroll_to_playhead) {
            // hard snap after seek/loop
            m_playback_state.force_scroll_to_playhead = false;
            m_scroll_state.scroll_pos = static_cast<double>(target);
            m_scroll_state.scroll_pos_valid = true;
            if (current != target) {
                m_window->hscroll_pianoRoll->setValue(target);
            }
        } else if (target < current) {
            // never scroll backwards during autoscroll
            target = current;
        }
        if (!m_scroll_state.scroll_pos_valid) {
            // seed interpolation state
            m_scroll_state.scroll_pos = static_cast<double>(
                m_playback_state.force_scroll_to_playhead ? target : current
            );
            m_scroll_state.scroll_pos_valid = true;
        }
        // smooth toward target
        const double alpha = 0.75;
        m_scroll_state.scroll_pos = m_scroll_state.scroll_pos
                                    + (static_cast<double>(target)
                                    - m_scroll_state.scroll_pos) * alpha;
        int scroll_value = qRound(m_scroll_state.scroll_pos);
        if (current != scroll_value) {
            m_window->hscroll_pianoRoll->setValue(scroll_value);
        }
    }

    // enforce song-end stop state
    if (m_playback_state.song_duration_ticks > 0
     && current_tick >= m_playback_state.song_duration_ticks) {
        this->stop();
        if (m_window->Button_Play) m_window->Button_Play->setChecked(false);
        if (m_window->Button_Pause) m_window->Button_Pause->setChecked(false);
        if (m_window->Button_Stop) m_window->Button_Stop->setChecked(true);
        if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(true);
        m_playback_state.edits_enabled = true;
        if (m_piano_roll) m_piano_roll->setEditsEnabled(true);
    }

    // update mixer meters + track voice status
    if (mixer) {
        Mixer::MeterLevels levels;
        mixer->getMeterLevels(&levels);
        if (m_master_meter) {
            m_master_meter->setLevels(levels.master_l, levels.master_r);
        }
        int first_row = 0;
        int last_row = g_num_midi_channels - 1;
        if (m_window && m_window->view_trackList) {
            // limit work to visible track rows
            int scroll_y = m_window->view_trackList->verticalScrollBar()->value();
            int viewport_h = m_window->view_trackList->viewport()->height();
            first_row = qMax(0, scroll_y / ui_track_item_height);
            last_row = qMin(g_num_midi_channels - 1,
                            first_row + (viewport_h / ui_track_item_height) + 1);
        }

        // push channel meter + activity state
        for (int ch = first_row; ch <= last_row; ++ch) {
            auto info = mixer->getChannelPlayInfo(ch);
            if (info.active) {
                m_track_roll->setTrackPlayingInfo(ch, info.voice_type);
            } else {
                m_track_roll->setTrackPlayingInfo(ch, QString());
            }
            m_track_roll->setTrackMeterLevels(ch, levels.channel_l[ch],
                                                  levels.channel_r[ch]);
        }
    }
}

void Controller::setMinimap(MinimapWidget *minimap) {
    m_minimap = minimap;
}

void Controller::setTrackEventViewMask(uint32_t mask) {
    if (!m_track_roll) return;
    m_track_roll->setEventViewMask(static_cast<TrackEventViewMask>(mask));
}

void Controller::setTrackEventPreset(int preset_index) {
    if (!m_track_roll) return;
    m_track_roll->setEventPreset(preset_index);
}

int Controller::currentSongIndex() const {
    return m_playback_state.current_song_index;
}

/**
 * Open a song in the song list by row number.
 */
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
    m_playback_state.current_song_index = index;
    songListSongRequested(next);
    return true;
}

/**
 * Open a song in the song list by song title.
 */
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
            m_playback_state.current_song_index = row;
            songListSongRequested(idx);
            return true;
        }
    }
    return false;
}

/**
 * Play the current song in the player.
 */
void Controller::play() {
    const VoiceGroup *voicegroup = nullptr;
    const QMap<QString, VoiceGroup> *all_voicegroups = nullptr;
    const QMap<QString, Sample> *samples = nullptr;
    const QMap<QString, QByteArray> *pcm_data = nullptr;
    const QMap<QString, KeysplitTable> *keysplit_tables = nullptr;

    // get instrument data and other sound samples from the project and give to the player
    if (m_song && m_project) {
        voicegroup = m_project->getVoiceGroup(m_song->getMetaInfo().voicegroup);
        all_voicegroups = &m_project->getVoiceGroups();
        samples = &m_project->getSamples();
        pcm_data = &m_project->getPcmData();
        keysplit_tables = &m_project->getKeysplitTables();
    }

    m_player->loadSong(m_song.get(), voicegroup, all_voicegroups,
                       samples, pcm_data, keysplit_tables);

    // update player playback position from the measure roll
    m_playback_state.playback_start_tick = m_measure_roll->tick();
    m_player->seekToTick(m_playback_state.playback_start_tick);

    m_scroll_state.autoscroll_enabled = true;
    m_scroll_state.scroll_pos_valid = false;
    m_playback_state.last_play_tick = -1;
    m_playback_state.force_scroll_to_playhead = true;
    m_playback_state.edits_enabled = false;
    m_player->play();

    // star timers
    m_player_elapsed.start();
    m_player_timer.start(16);

    // disable edits while a song is playing, since that adds a huge complexity
    if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(false);
    if (m_piano_roll) m_piano_roll->setEditsEnabled(false);
    if (m_track_roll) m_track_roll->setTracksInteractive(false);
}

/**
 * Stop playback of the song (if applicable), and reset the playhead to tick 0.
 */
void Controller::stop() {
    this->pause();
    this->seekToStart();
}

/**
 * Pause the playback of the song, but keep playhead position.
 */
void Controller::pause() {
    m_player->stop();
    m_player_timer.stop();

    m_playback_state.last_play_tick = -1;
    m_playback_state.force_scroll_to_playhead = false;
    m_playback_state.edits_enabled = true;
    m_track_roll->clearAllPlayingInfo();
    m_track_roll->clearAllMeters();
    if (m_master_meter) {
        m_master_meter->setLevels(0.0f, 0.0f);
    }

    // re-enable edits
    if (m_window->ButtonBox_Tools) m_window->ButtonBox_Tools->setEnabled(true);
    if (m_piano_roll) m_piano_roll->setEditsEnabled(true);
    if (m_track_roll) m_track_roll->setTracksInteractive(true);
}

int Controller::currentTick() const {
    if (!m_song) return 0;
    // when not playing, the sequencer time can be invald so use the ui playhead instead
    if (!m_player_timer.isActive()) {
        return m_measure_roll ? m_measure_roll->tick() : 0;
    }
    double current_seconds = m_player->getSequencer()->getCurrentTime();
    return m_song->getTickFromTime(current_seconds);
}

/**
 * Move the playhead to a specified tick.
 * 
 * Synchronizes playback and scroll state to reflect the new state.
 */
void Controller::seekToTick(int tick) {
    bool was_playing = m_player_timer.isActive();

    m_measure_roll->setTick(tick);
    m_measure_roll->updatePlaybackGuide(tick);
    m_scroll_state.autoscroll_enabled = true;

    if (was_playing) {
        m_playback_state.playback_start_tick = tick;
        m_player->seekToTick(tick);
        m_player_elapsed.restart();
    }
    m_scroll_state.scroll_pos_valid = false;
    m_playback_state.last_play_tick = tick;
    m_playback_state.force_scroll_to_playhead = true;
}

void Controller::seekToStart() {
    this->seekToTick(0);
}

/**
 * Sets a volume for the application, uncoupled from the actual song volume.
 */
void Controller::setMasterVolume(int value) {
    if (!m_player) return;
    float volume = static_cast<float>(value) / 100.0f;
    m_player->getMixer()->setMasterVolume(volume);
}

void Controller::setMasterMeter(MasterMeterWidget *meter) {
    m_master_meter = meter;
}

/**
 * Precomputes key signature inferences for songs without mappings.
 * Called on song load, so that this does not need to be called every ui update.
 */
void Controller::rebuildInferredKeySignatureCache() {
    m_inferred_key_by_tick.clear();
    if (!m_song) return;
    if (!m_song->getKeySignatures().isEmpty()) return;

    int duration = m_playback_state.song_duration_ticks;
    if (duration <= 0) duration = m_song->durationInTicks();
    if (duration < 0) duration = 0;

    int tpqn = m_song->getTicksPerQuarterNote();
    if (tpqn <= 0) tpqn = 48;

    for (int tick = 0; tick <= duration; tick += tpqn) {
        const KeySignatureState key_sig = inferKeySigFromContext(m_song.get(), tick);
        m_inferred_key_by_tick.insert(tick, {key_sig.sharps_flats, key_sig.is_minor});
    }
    if (!m_inferred_key_by_tick.contains(duration)) {
        const KeySignatureState key_sig = inferKeySigFromContext(m_song.get(), duration);
        m_inferred_key_by_tick.insert(duration, {key_sig.sharps_flats, key_sig.is_minor});
    }
    if (m_inferred_key_by_tick.isEmpty()) {
        m_inferred_key_by_tick.insert(0, {0, false});
    }
}

QPair<int, bool> Controller::keySignatureForTick(int tick) const {
    if (m_song && !m_song->getKeySignatures().isEmpty()) {
        return keySignatureAtTick(m_song.get(), tick);
    }
    if (m_inferred_key_by_tick.isEmpty()) {
        return {0, false};
    }
    auto it = m_inferred_key_by_tick.upperBound(tick);
    if (it == m_inferred_key_by_tick.begin()) {
        return it.value();
    }
    --it;
    return it.value();
}

/**
 * Take the current song data and update the box at the top of the screen where the
 * timer and measure information is diplayed. Called only when loading songs.
 */
void Controller::updateSongMetaDisplay() {
    if (!m_window || !m_song) return;

    // placeholder defaults
    QString tempo_text = "120.0 BPM";
    QString time_sig_text = "4/4";
    QString key_text = "?";

    // populate song info form fields
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
        m_window->label_SongInfoTPQNValue->setText(QString::number(
                                                   m_song->getTicksPerQuarterNote()));
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

    // resolve initial tempo text
    auto &tempos = m_song->getTempoChanges();
    if (!tempos.isEmpty()) {
        auto it = tempos.begin();
        smf::MidiEvent *e = it.value();
        if (e) {
            tempo_text = QString("%1 BPM").arg(e->getTempoBPM(), 0, 'f', 1);
        }
    }

    // resolve initial time signature text
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

    // resolve initial key signature text
    auto &keys = m_song->getKeySignatures();
    if (!keys.isEmpty()) {
        auto it = keys.begin();
        smf::MidiEvent *e = it.value();
        if (e) {
            int sharps_flats = static_cast<int>(static_cast<int8_t>((*e)[3]));
            int is_minor = (*e)[4];
            key_text = keySignatureToString(sharps_flats, is_minor);
        }
    } else {
        const auto key_sig = this->keySignatureForTick(0);
        key_text = keySignatureToString(key_sig.first, key_sig.second);
    }

    // set meta display values into ui
    if (m_window->label_MetaTempoValue) {
        m_window->label_MetaTempoValue->setText(tempo_text);
    }
    if (m_window->label_MetaTimeSigValue) {
        m_window->label_MetaTimeSigValue->setText(time_sig_text);
    }
    if (m_window->label_MetaKeyValue) {
        m_window->label_MetaKeyValue->setText(key_text);
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

/**
 * Update the meta info box based on values at the current tick during playback.
 */
void Controller::updateSongPositionDisplay(int tick) {
    // skip if ui/song unavailable or unchanged tick
    if (!m_window || !m_song) return;
    if (tick == m_playback_state.last_meta_tick) return;
    m_playback_state.last_meta_tick = tick;

    // guard for missing song duration
    int end_tick = m_playback_state.song_duration_ticks;
    if (end_tick <= 0) {
        if (m_window->label_MetaTickValue) {
            m_window->label_MetaTickValue->setText(QString::number(0));
        }
        if (m_window->label_MetaMeasureBeatValue) {
            m_window->label_MetaMeasureBeatValue->setText("1.1");
        }
        const auto key_sig = this->keySignatureForTick(0);
        const QString key_text = keySignatureToString(key_sig.first, key_sig.second);
        if (m_window->label_MetaKeyValue) {
            m_window->label_MetaKeyValue->setText(key_text);
        }
        return;
    }

    // clamp tick into song bounds
    if (tick >= end_tick) {
        tick = end_tick - 1;
        if (tick < 0) tick = 0;
    }

    int measure_number = 1;
    int beat_number = 1;

    // walk time-signature segments to compute measure.beat
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

            int end_of_segment = m_playback_state.song_duration_ticks;
            if (it_sig != end_sig) {
                auto next_it = it_sig;
                ++next_it;
                if (next_it != end_sig) {
                    end_of_segment = next_it.key();
                }
            }

            while (current_tick < end_of_segment) {
                int measure_start = current_tick;
                int measure_end = std::min(current_tick + ticks_per_measure,
                                           end_of_segment);

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

    // write live tick/measure/key labels
    if (m_window->label_MetaTickValue) {
        m_window->label_MetaTickValue->setText(QString::number(tick));
    }
    if (m_window->label_MetaMeasureBeatValue) {
        m_window->label_MetaMeasureBeatValue->setText(QString("%1.%2")
                                                      .arg(measure_number)
                                                      .arg(beat_number));
    }
    const auto key_sig = this->keySignatureForTick(tick);
    const QString key_text = keySignatureToString(key_sig.first, key_sig.second);
    if (m_window->label_MetaKeyValue) {
        m_window->label_MetaKeyValue->setText(key_text);
    }
}

/**
 * Controller::eventFilter
 * 
 * A filter for scrolling and clicking events in the measure views and event rolls.
 * - For a user's horizontal scroll: allow overriding the auto scroll, set debounce timer.
 * - When a user clicks on the measure view, set playhead to the nearest measure
 *   or beat boundary for the active time signature.
 */
bool Controller::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->angleDelta().x() != 0) {
            m_scroll_state.autoscroll_enabled = false;
            m_scroll_state.scroll_debounce.start();
            m_scroll_state.scroll_pos_valid = false;
        }
    }

    if (watched == m_window->view_measures->viewport()
     && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);

        // the time-range tool needs the measure scene to receive the click itself,
        // so do not consume it here for playhead seeking
        if (m_window->button_select_time && m_window->button_select_time->isChecked()) {
            return QObject::eventFilter(watched, event);
        }

        if (mouse_event->button() == Qt::LeftButton && m_song) {
            QPointF scene_pos = m_window->view_measures->mapToScene(mouse_event->pos());

            int tick = static_cast<int>(scene_pos.x() / ui_tick_x_scale);
            int snapped_tick = snapMeasureViewTick(m_song.get(), tick, scene_pos.y());
            int end_tick = m_playback_state.song_duration_ticks;
            if (end_tick <= 0 && m_song) end_tick = m_song->durationInTicks();
            snapped_tick = qBound(0, snapped_tick, end_tick);

            this->seekToTick(snapped_tick);
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

/**
 * For the Event Info GroupBox to the right of the piano roll view.
 * Displays information from a specific event (generally the first-selected event).
 * 
 * !TODO: update the ui, it's shit
 */
void Controller::displayEvent(smf::MidiEvent *event) {
    // !TODO: other safety checks? (isNote / isNoteOn, etc)
    if (!event) return;

    // clamp tick spinbox ranges to song duration
    const int max_tick = (m_playback_state.song_duration_ticks > 0) ?
                          m_playback_state.song_duration_ticks :
                          std::numeric_limits<int>::max();
    m_window->spinBox_NoteOnTick->setMaximum(max_tick);
    m_window->spinBox_NoteOffTick->setMaximum(max_tick);

    // update note-name key signature context
    if (m_window->spinBox_NoteKey) {
        MidiKeySpinBox *key_box = m_window->spinBox_NoteKey;
        KeySignatureState key_sig;
        const auto from_meta = keySignatureAtTick(m_song.get(), event->tick);
        if (m_song && !m_song->getKeySignatures().isEmpty()) {
            key_sig.sharps_flats = from_meta.first;
            key_sig.is_minor = from_meta.second;
            key_sig.has_meta_signature = true;
        } else {
            key_sig = inferKeySigFromContext(m_song.get(), event->tick, event->track);
        }
        key_box->setKeySignature(key_sig.sharps_flats, key_sig.is_minor);
    }

    // populate core note event fields
    m_window->spinBox_NoteChannel->setValue(event->getChannel());
    m_window->spinBox_NoteKey->setValue(event->getKeyNumber());
    m_window->spinBox_NoteOnTick->setValue(event->tick);
    m_window->spinBox_NoteOnVelocity->setValue(event->getVelocity());

    // resolve linked note-off values
    if (event->hasLink()) {
        m_window->spinBox_NoteOffTick->setValue(event->getLinkedEvent()->tick);
        m_window->spinBox_NoteOffVelocity->setValue(
            event->getLinkedEvent()->getVelocity()
        );
    } else {
        m_window->spinBox_NoteOffTick->setValue(event->tick);
        m_window->spinBox_NoteOffVelocity->setValue(0);
    }

    // show display-row track index
    if (m_window->lineEdit_EventTrackValue) {
        int display_row = m_song ? m_song->getDisplayRow(event->track) : -1;
        m_window->lineEdit_EventTrackValue->setText(display_row >= 0 ?
                                                    QString::number(display_row) : "-");
    }

    // resolve active program at this event
    uint8_t program = programForEvent(m_song.get(), event);
    if (m_window->lineEdit_EventProgramValue) {
        m_window->lineEdit_EventProgramValue->setText(QString::number(program));
    }

    // resolve final instrument after keysplits/drumsets
    const Instrument *resolved = nullptr;
    if (m_song && m_project) {
        const VoiceGroup *voicegroup = m_project->getVoiceGroup(
            m_song->getMetaInfo().voicegroup
        );
        const QMap<QString, VoiceGroup> *all_voicegroups = &m_project->getVoiceGroups();
        const QMap<QString, KeysplitTable> *keysplits = &m_project->getKeysplitTables();
        if (voicegroup && program < voicegroup->instruments.size()) {
            const Instrument *base = &voicegroup->instruments[program];
            resolved = resolveInstrumentForEvent(
                                    base,
                                    static_cast<uint8_t>(event->getKeyNumber()),
                                    all_voicegroups,
                                    keysplits
            );
        }
    }

    // fill derived playback metadata fields
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

/**
 * Response to listView_songTable double clicks and song load programmatic requests.
 * Performs the actual loading 
 */
void Controller::songListSongRequested(const QModelIndex &index) {
    // resolve target song title from model role
    QString title = index.data(Qt::UserRole).toString();
    if (title.isEmpty()) {
        return;
    }

    // synchronize list selection and stored song index
    if (m_window && m_window->listView_songTable
     && m_window->listView_songTable->model()) {
        auto *view = m_window->listView_songTable;
        auto *model = view->model();
        bool matched = false;
        const int rows = model->rowCount();
        for (int row = 0; row < rows; ++row) {
            QModelIndex idx = model->index(row, 0);
            if (!idx.isValid()) continue;
            if (idx.data(Qt::UserRole).toString() == title) {
                view->setCurrentIndex(idx);
                m_playback_state.current_song_index = row;
                matched = true;
                break;
            }
        }
        if (!matched) {
            m_playback_state.current_song_index = index.row();
        }
    } else {
        m_playback_state.current_song_index = index.row();
    }

    // notify a song was requested
    emit songSelected(title);

    // fetch editable song entry metadata
    SongEntry &entry = m_project->getSongEntryByTitle(title);

    // abort if this entry has no midi path
    if (entry.midifile.isEmpty()) {
        logging::warn(QString("No MIDI file defined for %1.").arg(title),
                      logging::LogCategory::Project);
        return;
    }

    // load midi into project if not loaded yet
    if (!m_project->songLoaded(title)) {
        smf::MidiFile midi = m_interface->loadMidi(title);

        // invalidate entry if midi could not be parsed
        if (midi.getTrackCount() == 0
         || (midi.getTrackCount() > 0
         && midi[0].size() == 0)) {
            logging::warn(QString("MIDI file for %1 could not be loaded.").arg(title),
                          logging::LogCategory::Project);
            entry.midifile.clear();
            if (m_song_list_model) {
                QModelIndex idx = m_song_list_model->index(index.row(), 0);
                emit m_song_list_model->dataChanged(idx, idx);
            }
            return;
        }

        // register newly loaded song and refresh model state
        m_project->addSong(title, midi);
        if (m_song_list_model) {
            m_song_list_model->refreshAll();
        }
    }

    // set active song and refresh icon/status state
    m_project->setActiveSong(title);
    if (m_song_list_model) {
        m_song_list_model->refreshAll();
    }

    // load song into ui/audio
    this->loadSong();
    if (m_song_editor) {
        QString error;
        if (!m_song_editor->setActiveSong(title, &error)) {
            logging::warn(QString("Failed to bind song editor: %1").arg(error),
                          logging::LogCategory::General);
        }
    }
}

/**
 * Toggle single channel mute state.
 * Additional caveat is that mute is non-exclusive, meaning if any solo is active,
 * clear it and keep current mute state.
 */
void Controller::onTrackMuteToggled(int channel, bool muted) {
    if (m_track_roll->hasSoloed()) {
        m_track_roll->clearAllSoloed();
    }

    m_track_roll->setTrackMuted(channel, muted);
    m_player->getMixer()->setChannelMute(channel, muted);
}

/**
 * Toggle a single channel exclusive solo state.
 */
void Controller::onTrackSoloToggled(int channel, bool soloed) {
    if (soloed) {
        // solo mutes all other tracks, and clears any other solo state
        m_track_roll->clearAllSoloed();
        m_track_roll->setTrackSoloed(channel, true);

        m_player->getMixer()->setAllMuted(true);
        m_player->getMixer()->setChannelMute(channel, false);

        // update ui
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
        m_piano_roll->setActiveTrack(track);
    }
}

void Controller::onTrackRightClicked(int track) {
    if (m_piano_roll) {
        m_piano_roll->setActiveTrack(track);
    }
    if (m_piano_roll) {
        m_piano_roll->selectNotesForTrack(track, true);
    }
}

void Controller::onAddTrackClicked() {
    if (!m_song_editor) {
        return;
    }

    QString error;
    if (!m_song_editor->addTrack(&error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onDeleteTrackClicked(int track) {
    if (!m_song_editor || !m_song) {
        return;
    }

    if (!m_song->isTrackEmpty(track) && m_window) {
        QMessageBox::StandardButton answer = QMessageBox::question(
            static_cast<MainWindow *>(this->parent()),
            QStringLiteral("Delete Track"),
            QStringLiteral("Delete this track and all of its events?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QString error;
    if (!m_song_editor->deleteTrack(track, &error) && !error.isEmpty()) {
        logging::warn(error, logging::LogCategory::Ui);
    }
}

void Controller::onTimeRangeSelected(int start_tick, int end_tick, bool modify) {
    if (m_piano_roll) {
        m_piano_roll->selectNotesInTickRange(start_tick, end_tick, modify);
    }
}

void Controller::onTimeSelectionCleared() {
    if (m_piano_roll) {
        m_piano_roll->selectEvents({});
    }
}

/**
 * Keeps the two track-related views in sync with the current scene geometry.
 */
void Controller::onTrackLayoutChanged() {
    if (!m_window || !m_track_roll) return;
    if (m_window->view_trackRoll) {
        const QRectF roll_rect = m_track_roll->sceneRoll()->sceneRect();
        m_window->view_trackRoll->setSceneRect(0.0, 0.0,
                                               roll_rect.width(), roll_rect.height());
    }
    if (m_window->view_trackList) {
        const QRectF list_rect = m_track_roll->sceneTracks()->sceneRect();
        m_window->view_trackList->setSceneRect(0.0, 0.0,
                                               list_rect.width(), list_rect.height());
    }
}
