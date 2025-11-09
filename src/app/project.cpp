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

void Project::addInstrumentToGroup(const QString &group, const Instrument &inst) {
    this->m_voicegroups[group].append(inst);
}

void Project::addTable(const QString &label, const KeysplitTable &table) {
    this->m_keysplit_tables[label] = table;
}

void Project::addSongEntry(const SongEntry &song) {
    qDebug() << "add song:" << song.title;
    this->m_song_table_order.append(song.title);
    this->m_song_entries[song.title] = song;
}

// since the song table and make rules are in different places, update sont entries with new data from make rules
// !TODO: unify into json? obviously that would be ideal unless there is resitance
void Project::updateSongData(const QString &title, const QString &voicegroup, int volume, int priority, int reverb) {
    if (this->m_song_entries.contains(title)) {
        // don't need to worry about extranneous rules, or anything that is not in the song table (right?)
        SongEntry &song = this->m_song_entries[title];

        song.voicegroup = voicegroup;
        song.volume = volume;
        song.priority = priority;
        song.reverb = reverb;

        song.midifile = title + ".mid";
    }
}
