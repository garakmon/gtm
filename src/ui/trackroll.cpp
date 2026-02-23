#include "ui/trackroll.h"

#include "sound/song.h"
#include "ui/colors.h"
#include "ui/graphicstrackitem.h"
#include "util/constants.h"
#include "util/util.h"



static TrackEventViewMask presetToMask(TrackRoll::TrackEventPreset preset) {
    switch (preset) {
    case TrackRoll::TrackEventPreset::All:
        return TrackEventView_All;
    case TrackRoll::TrackEventPreset::Mix:
        return TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan;
    case TrackRoll::TrackEventPreset::Timbre:
        return TrackEventView_Program;
    case TrackRoll::TrackEventPreset::Other:
        return TrackEventView_ControlOther;
    case TrackRoll::TrackEventPreset::Custom:
    default:
        return 0;
    }
}

TrackRoll::TrackRoll(QObject *parent) : QObject(parent) {
    m_scene_roll.setParent(this);
}

/**
 * Set the active song and rebuild the track scenes.
 */
void TrackRoll::setSong(std::shared_ptr<Song> song) {
    this->reset();
    m_active_song = song;
    this->drawTracks();
}

/**
 * Update the playback label for one track.
 */
void TrackRoll::setTrackPlayingInfo(int channel, const QString &voiceType) {
    auto it = m_track_items.find(channel);
    if (it == m_track_items.end() || !it.value()) return;

    it.value()->setPlayingInfo(voiceType);
}

/**
 * Clear the playback label for every track.
 */
void TrackRoll::clearAllPlayingInfo() {
    for (auto item : m_track_items) {
        item->clearPlayingInfo();
    }
}

/**
 * Update the level meter for one track.
 */
void TrackRoll::setTrackMeterLevels(int channel, float left, float right) {
    auto it = m_track_items.find(channel);
    if (it == m_track_items.end() || !it.value()) return;

    it.value()->setMeterLevels(left, right);
}

void TrackRoll::clearAllMeters() {
    for (auto item : m_track_items) {
        item->setMeterLevels(0.0f, 0.0f);
    }
}

void TrackRoll::setTrackMuted(int channel, bool muted) {
    auto it = m_track_items.find(channel);
    if (it == m_track_items.end() || !it.value()) return;

    it.value()->setMuted(muted);
}

void TrackRoll::setTrackSoloed(int channel, bool soloed) {
    auto it = m_track_items.find(channel);
    if (it == m_track_items.end() || !it.value()) return;

    it.value()->setSoloed(soloed);
}

void TrackRoll::clearAllSoloed() {
    for (auto item : m_track_items) {
        item->setSoloed(false);
    }
}

bool TrackRoll::hasSoloed() const {
    for (auto item : m_track_items) {
        if (item->isSoloed()) return true;
    }

    return false;
}

void TrackRoll::setTracksInteractive(bool enabled) {
    for (auto item : m_track_items) {
        if (item) item->setControlsEnabled(enabled);
    }
}

/**
 * Toggle the expanded state for one display row.
 */
void TrackRoll::toggleTrackExpansion(int display_row) {
    if (m_expanded_row == display_row) {
        m_expanded_row = -1;
    } else {
        m_expanded_row = display_row;
    }

    this->relayout();
}

/**
 * Set the visible track event mask.
 */
void TrackRoll::setEventViewMask(TrackEventViewMask mask) {
    TrackEventViewMask normalized = mask == 0 ? TrackEventView_All : mask;
    if (m_event_view_mask == normalized) return;

    m_event_view_mask = normalized;

    for (auto *manager : m_roll_managers) {
        if (manager) manager->setEventViewMask(m_event_view_mask);
    }
}

/**
 * Set the visible track event preset.
 */
void TrackRoll::setEventPreset(TrackEventPreset preset) {
    if (preset == TrackEventPreset::Custom) return;

    TrackEventViewMask mask = presetToMask(preset);
    this->setEventViewMask(mask);
}

/**
 * Set the visible track event preset from the combobox index.
 */
void TrackRoll::setEventPreset(int preset_index) {
    if (preset_index < static_cast<int>(TrackEventPreset::All) ||
        preset_index > static_cast<int>(TrackEventPreset::Other)) {
        return;
    }

    this->setEventPreset(static_cast<TrackEventPreset>(preset_index));
}

/**
 * Set the instrument lookup data for track events.
 */
void TrackRoll::setInstrumentContext(const VoiceGroup *song_voicegroup,
                                     const QMap<QString, VoiceGroup> *all_voicegroups,
                                     const QMap<QString, KeysplitTable> *keysplits) {
    m_song_voicegroup = song_voicegroup;
    m_all_voicegroups = all_voicegroups;
    m_keysplit_tables = keysplits;
}

/**
 * Handle a click on one track row.
 */
void TrackRoll::onTrackClicked(int track) {
    for (auto it = m_track_items.begin(); it != m_track_items.end(); ++it) {
        if (it.value()->track() == track) {
            this->toggleTrackExpansion(it.key());
            break;
        }
    }

    emit trackSelected(track);
}

/**
 * Clear the current track scenes and cached items.
 */
void TrackRoll::reset() {
    m_scene_track_list.clear();
    m_scene_roll.clear();
    m_track_items.clear();
    m_roll_managers.clear();
    m_expanded_row = -1;
}

/**
 * Draw every track row for the active song.
 */
void TrackRoll::drawTracks() {
    if (!m_active_song) return;

    const auto *states = m_active_song->getInitialChannelStates();

    int track_num = 0;
    int display_row = 0;
    for (auto track : m_active_song->tracks()) {
        if (m_active_song->isMetaTrack(track_num)) {
            track_num++;
            continue;
        }

        GraphicsTrackItem *track_item = new GraphicsTrackItem(track_num, display_row);
        m_scene_track_list.addItem(track_item);

        // fall back to channel 0 state beyond 16 rows (m4a engine limit)
        int ch = display_row < 16 ? display_row : 0;
        auto *manager = new GraphicsTrackRollManager(
            track, track_item,
            states[ch].volume, states[ch].pan,
            states[ch].expression, states[ch].pitch_bend,
            m_song_voicegroup, m_all_voicegroups, m_keysplit_tables);
        manager->setEventViewMask(m_event_view_mask);
        m_scene_roll.addItem(manager);

        connect(track_item, &GraphicsTrackItem::muteToggled,
                this, &TrackRoll::trackMuteToggled, Qt::UniqueConnection);
        connect(track_item, &GraphicsTrackItem::soloToggled,
                this, &TrackRoll::trackSoloToggled, Qt::UniqueConnection);
        connect(track_item, &GraphicsTrackItem::trackClicked,
                this, &TrackRoll::onTrackClicked, Qt::UniqueConnection);

        m_roll_managers[display_row] = manager;
        m_track_items[display_row] = track_item;

        track_num++;
        display_row++;
    }

    this->relayout();
}

/**
 * Recompute the row geometry after expansion changes.
 */
void TrackRoll::relayout() {
    int y = 0;
    int num_rows = m_track_items.size();

    for (int row = 0; row < num_rows; row++) {
        auto track_it = m_track_items.find(row);
        if (track_it == m_track_items.end() || !track_it.value()) continue;

        bool expanded = row == m_expanded_row;
        GraphicsTrackItem *track_item = track_it.value();
        track_item->setExpanded(expanded);
        track_item->setYPosition(y);

        auto manager_it = m_roll_managers.find(row);
        if (manager_it != m_roll_managers.end() && manager_it.value()) {
            GraphicsTrackRollManager *manager = manager_it.value();
            manager->setExpanded(expanded);
            manager->setPos(0, y);
        }

        y += ui_track_item_height;
        if (expanded) {
            y += ui_controller_total_height;
        }
    }

    m_scene_track_list.setSceneRect(m_scene_track_list.itemsBoundingRect());
    m_scene_roll.setSceneRect(m_scene_roll.itemsBoundingRect());
    emit layoutChanged();
}
