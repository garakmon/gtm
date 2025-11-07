#include "project.h"

#include <QDebug>
#include <QFile>
#include <sstream>
//#include <sstringstream>

#include "MidiFile.h"
#include "projectinterface.h"



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

    // add song to song table
    m_song_table.insert("mus_title", std::make_shared<Song>(midifile));

    for (auto t : m_song_table["mus_title"]->tracks()) {
        //
        qDebug() << "track size" << t->getSize();
    }

    // qDebug() << "OUTPUT FILE:\n==============================";
    // std::cout << midifile;

    this->m_active_song = this->m_song_table.values().first();
}

void Project::addSampleMapping(const QString &label, const QString &path) {
    this->m_sample_map.insert(label, path);
}

QString Project::getSamplePath(const QString &label) const {
    return this->m_sample_map.value(label, "");
}
