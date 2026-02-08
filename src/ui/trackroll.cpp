#include "trackroll.h"

#include <QGraphicsView>
#include <QEvent>
#include <QWheelEvent>

#include "graphicstrackitem.h"
#include "song.h"
#include "constants.h"
#include "colors.h"
#include "util.h"

namespace {
TrackEventViewMask presetToMask(TrackRoll::TrackEventPreset preset) {
    switch (preset) {
    case TrackRoll::TrackEventPreset::All:
        return kTrackEventView_All;
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

TrackRoll::TrackEventPreset maskToPreset(TrackEventViewMask mask) {
    if (mask == kTrackEventView_All) return TrackRoll::TrackEventPreset::All;
    if (mask == (TrackEventView_Volume | TrackEventView_Expression | TrackEventView_Pan)) return TrackRoll::TrackEventPreset::Mix;
    if (mask == TrackEventView_Program) return TrackRoll::TrackEventPreset::Timbre;
    if (mask == TrackEventView_ControlOther) return TrackRoll::TrackEventPreset::Other;
    return TrackRoll::TrackEventPreset::Custom;
}
} // namespace


TrackRoll::TrackRoll(QObject *parent) : QObject(parent) {
    this->m_scene_roll.setParent(this);
}

void TrackRoll::drawTracks() {
    this->m_scene_track_list.clear();
    this->m_scene_roll.clear();
    this->m_track_items.clear();
    this->m_roll_managers.clear();
    this->m_expanded_row = -1;

    const auto *states = m_active_song->getInitialChannelStates();

    int track_num = 0;
    int display_row = 0;
    for (auto track : this->m_active_song->tracks()) {
        if (this->m_active_song->isMetaTrack(track_num)) {
            track_num++;
            continue;
        }

        GraphicsTrackItem *track_item = new GraphicsTrackItem(track_num, display_row);
        this->m_scene_track_list.addItem(track_item);

        // Pass initial channel state for step graph data
        int ch = display_row < 16 ? display_row : 0;
        auto *manager = new GraphicsTrackRollManager(track, track_item,
            states[ch].volume, states[ch].pan, states[ch].expression, states[ch].pitch_bend);
        manager->setEventViewMask(m_event_view_mask);
        this->m_scene_roll.addItem(manager);
        m_roll_managers[display_row] = manager;

        // Connect signals
        connect(track_item, &GraphicsTrackItem::muteToggled, this, &TrackRoll::trackMuteToggled);
        connect(track_item, &GraphicsTrackItem::soloToggled, this, &TrackRoll::trackSoloToggled);
        connect(track_item, &GraphicsTrackItem::trackClicked, this, &TrackRoll::onTrackClicked);

        // Store by display row (which maps to MIDI channel for typical GBA music)
        m_track_items[display_row] = track_item;

        track_num++;
        display_row++;
    }

    relayout();
}

void TrackRoll::setTrackPlayingInfo(int channel, const QString &voiceType) {
    // Channel typically maps to display row for GBA music
    if (m_track_items.contains(channel)) {
        m_track_items[channel]->setPlayingInfo(voiceType);
    }
}

void TrackRoll::clearAllPlayingInfo() {
    for (auto item : m_track_items) {
        item->clearPlayingInfo();
    }
}

void TrackRoll::setTrackMuted(int channel, bool muted) {
    if (m_track_items.contains(channel)) {
        m_track_items[channel]->setMuted(muted);
    }
}

void TrackRoll::setTrackSoloed(int channel, bool soloed) {
    if (m_track_items.contains(channel)) {
        m_track_items[channel]->setSoloed(soloed);
    }
}

void TrackRoll::setTrackMeterLevels(int channel, float left, float right) {
    if (m_track_items.contains(channel)) {
        m_track_items[channel]->setMeterLevels(left, right);
    }
}

void TrackRoll::clearAllMeters() {
    for (auto item : m_track_items) {
        item->setMeterLevels(0.0f, 0.0f);
    }
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

void TrackRoll::onTrackClicked(int track) {
    // Find display_row for this track number
    for (auto it = m_track_items.begin(); it != m_track_items.end(); ++it) {
        if (it.value()->track() == track) {
            toggleTrackExpansion(it.key());
            break;
        }
    }
    emit trackSelected(track);
}

void TrackRoll::toggleTrackExpansion(int display_row) {
    if (m_expanded_row == display_row) {
        m_expanded_row = -1;
    } else {
        m_expanded_row = display_row;
    }
    relayout();
}

void TrackRoll::setEventViewMask(TrackEventViewMask mask) {
    const TrackEventViewMask normalized = (mask == 0) ? kTrackEventView_All : mask;
    if (m_event_view_mask == normalized) return;
    m_event_view_mask = normalized;
    m_event_preset = maskToPreset(normalized);
    for (auto *manager : m_roll_managers) {
        if (manager) manager->setEventViewMask(m_event_view_mask);
    }
}

void TrackRoll::setEventPreset(TrackEventPreset preset) {
    if (preset == TrackEventPreset::Custom) return;
    const TrackEventViewMask mask = presetToMask(preset);
    setEventViewMask(mask);
}

void TrackRoll::relayout() {
    int y = 0;
    int num_rows = m_track_items.size();
    for (int row = 0; row < num_rows; row++) {
        if (!m_track_items.contains(row)) continue;

        bool expanded = (row == m_expanded_row);
        auto *track_item = m_track_items[row];
        track_item->setExpanded(expanded);
        track_item->setYPosition(y);

        if (m_roll_managers.contains(row)) {
            auto *manager = m_roll_managers[row];
            manager->setExpanded(expanded);
            manager->setPos(0, y);
        }

        y += ui_track_item_height;
        if (expanded) {
            y += ui_automation_total_height;
        }
    }

    m_scene_track_list.setSceneRect(m_scene_track_list.itemsBoundingRect());
    m_scene_roll.setSceneRect(m_scene_roll.itemsBoundingRect());
    emit layoutChanged();
}
