#include "song.h"


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

    double duration;
    int track_num = 0;
    for (auto track : this->tracks()) {
        qDebug() << "\n*** TRACK" << track_num << "***\n";
        bool meta_track = true;
        // handle the meta track uniquely
        if (track_num == 0) {
            for (int i = 0; i < track->size(); i++) {
                smf::MidiEvent *midi_event = &(*track)[i];
                if (midi_event->isTimeSignature()) {
                    this->m_time_signatures[midi_event->tick] = midi_event;
                }
            }
        }
        // else
        for (int i = 0; i < track->size(); i++) {
            smf::MidiEvent *midi_event = &(*track)[i];
            if (midi_event->isNote()) {
                //if (midi_event->isNoteOn()) this->m_piano_roll->addNote(track_num, midi_event);
                if (midi_event->isNoteOn()) this->m_notes.append({track_num, midi_event});
                meta_track = false;
                continue;
            }
            else if (midi_event->isMetaMessage()) {
                qDebug() << "MetaMessage: type" << midi_event->getMetaType();
            }
            else if (midi_event->isController()) {
                meta_track = false;
                qDebug() << "Controller:" << midi_event->getControllerNumber() << midi_event->getControllerValue();
            }
            else if (midi_event->isPatchChange()) {
                meta_track = false;
                qDebug() << "Patch Change:" << midi_event->getChannel() << midi_event->getP1();
            }
            else if (midi_event->isPressure()) {
                meta_track = false;
                qDebug() << "Pressure:" << midi_event->getP1();
            }
            else if (midi_event->isPitchbend()) {
                meta_track = false;
                qDebug() << "Pitch Bend:" << midi_event->getP1() << midi_event->getP2();
            }
            else {
                qDebug() << "unknown midi message type";
            }
        }
        track_num++;
    }
    qDebug() << "load sucess";
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
