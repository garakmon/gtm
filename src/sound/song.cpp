#include "song.h"


#include <algorithm>
#include <QDebug>



Song::Song() {
    //
}

Song::Song(smf::MidiFile &midifile) : smf::MidiFile(midifile) {
    // move to load()?
    //m_track_count = m_midi_file.getTrackCount();
    //this->linkNotePairs();
}

Song::~Song() {
    //
}

// TIME SIGNATURE CODE
/*
    // load meta events from the first track
    for (int i = 0; i < midifile[0].size(); i++) {
        if (midifile[0][i].isTimeSignature()) {
            TimeSigChange change;
            change.tick = midifile[0][i].tick;
            change.num = midifile[0][i].getP1();
            change.den = std::pow(2, midifile[0][i].getP2());
            timeSigMap.push_back(change);
        }
    }
*/

bool Song::load() {
    this->doTimeAnalysis();
    this->linkNotePairs();

    m_meta_tracks.clear();
    m_time_signatures.clear();
    m_tempo_changes.clear();
    m_key_signatures.clear();
    m_markers.clear();
    m_notes.clear();

    int track_num = 0;
    for (auto track : this->tracks()) {
        bool has_notes = false;

        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *midi_event = &(*track)[i];

            if (midi_event->isTimeSignature()) {
                m_time_signatures[midi_event->tick] = midi_event;
            }
            else if (midi_event->isTempo()) {
                m_tempo_changes[midi_event->tick] = midi_event;
            }
            else if (midi_event->isKeySignature()) {
                m_key_signatures[midi_event->tick] = midi_event;
            }
            else if (midi_event->isMarkerText() || midi_event->isText()) {
                m_markers.append(midi_event);
            }
            else if (midi_event->isNoteOn()) {
                m_notes.append({track_num, midi_event});
                has_notes = true;
            }
        }

        if (!has_notes) {
            m_meta_tracks.insert(track_num);
        }

        track_num++;
    }

    this->mergeEvents();
    return true;
}

void Song::mergeEvents() {
    // !TODO: whenever I get to letting edits happen, this probably needs
    // to be called whenever a Song is unclean/has been edited in order to play it back
    this->m_merged_events.clear();

    int track_count = this->getTrackCount();

    int total_events = 0;
    for (int t = 0; t < track_count; t++) {
        total_events += this->getEventCount(t);
    }

    this->m_merged_events.reserve(total_events);

    for (int track_index = 0; track_index < track_count; track_index++) {
        smf::MidiEventList &event_list = *this->m_events[track_index];
        int event_count = event_list.size();

        for (int i = 0; i < event_count; ++i) {
            this->m_merged_events.append(&event_list[i]);
        }
    }

    // qsort is deprecated apparently
    std::sort(this->m_merged_events.begin(), this->m_merged_events.end(),
        [](smf::MidiEvent *a, smf::MidiEvent *b) {
            if (a->tick == b->tick) {
                return a->track < b->track;
            }
            return a->tick < b->tick;
        }
    );
}

double Song::durationInSeconds() {
    this->doTimeAnalysis();
    this->joinTracks();
    double end_time = (*this)[0].last().seconds;
    this->splitTracks();
    return end_time;
}

int Song::durationInTicks() {
    this->joinTracks();
    int end_tick = (*this)[0].last().tick;
    this->splitTracks();
    return end_tick;
}

int Song::getTickFromTime(double seconds) {
    if (seconds <= 0.0) return 0;

    smf::MidiEventList &event_list = (*this)[0];
    int n_events = event_list.size();

    if (n_events == 0) return 0;

    // binary search to find the event closest to our target time
    int left = 0;
    int right = n_events - 1;
    int mid;

    while (left <= right) {
        mid = left + (right - left) / 2;
        if (event_list[mid].seconds < seconds) {
            left = mid + 1;
        } else if (event_list[mid].seconds > seconds) {
            right = mid - 1;
        } else {
            return event_list[mid].tick;
        }
    }

    // with no exact match, right is the index of the event immediately before the target time.
    if (right < 0) return 0;

    smf::MidiEvent &prev_event = event_list[right];

    // calculate the 'leftover' ticks between the last event and the target time based on the tempo
    double time_diff = seconds - prev_event.seconds;

    double spt = this->getFileDurationInSeconds() / this->getFileDurationInTicks();

    int extra_ticks = static_cast<int>(time_diff / spt);

    return prev_event.tick + extra_ticks;
}
