#include "project.h"

#include <QDebug>
#include <QFile>
#include <sstream>
//#include <sstringstream>

#include "MidiFile.h"



Project::Project() {
    //
}

Project::~Project() {
    //
}

void Project::load() {
    // TODO: temp reading resource file
    smf::MidiFile midifile;

    QFile f(":/media/mus_title.mid");
    f.open(QIODevice::ReadOnly);
    std::istringstream is(f.readAll().toStdString());
    
    midifile.read(is);

    qDebug().nospace() << "TPQ: " << midifile.getTicksPerQuarterNote();
    qDebug().nospace() << "TRACKS: " << midifile.getTrackCount();
    // midifile.joinTracks();
    // // midifile.getTrackCount() will now return "1", but original
    // // track assignments can be seen in .track field of MidiEvent.
    // qDebug().nospace() << "TICK    DELTA   TRACK   MIDI MESSAGE\n";
    // qDebug().nospace() << "____________________________________\n";
    // smf::MidiEvent* mev;
    // int deltatick;
    // for (int event=0; event < midifile[0].size(); event++) {
    //     mev = &midifile[0][event];
    //     if (event == 0) deltatick = mev->tick;
    //     else deltatick = mev->tick - midifile[0][event-1].tick;
    //     qDebug().nospace() << Qt::dec << mev->tick
    //       << '\t' << deltatick
    //       << '\t' << mev->track
    //       << '\t' << Qt::hex;
    //     for (int i=0; i < mev->size(); i++)
    //         qDebug().nospace() << (int)(*mev)[i] << ' ';
    //     //qDebug().nospace() << "\n";
    // }

    // add song to song table
    m_song_table.insert("mus_title", std::make_shared<Song>(midifile));

    for (auto t : m_song_table["mus_title"]->tracks()) {
        //
        qDebug() << "track size" << t->getSize();
    }

    this->m_active_song = this->m_song_table.values().first();
}
