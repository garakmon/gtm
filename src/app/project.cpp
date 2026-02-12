#include "app/project.h"

#include <QDebug>
#include <QFile>
#include <sstream>
//#include <sstringstream>

#include "MidiFile.h"
#include "app/projectinterface.h"



Project::Project() {
    //
}

Project::~Project() {
    //
}

void Project::load() {
    // TODO: what this do?
}

void Project::addSampleMapping(const QString &label, const QString &path) {
    this->m_sample_map.insert(label, path);
}

QString Project::getSamplePath(const QString &label) const {
    return this->m_sample_map.value(label, "");
}

void Project::addSample(const QString &label, const Sample &sample) {
    m_samples.insert(label, sample);
}

void Project::addPcmData(const QString &label, const QByteArray &data) {
    m_pcm_data.insert(label, data);
}

Sample &Project::getSample(const QString &label) {
    return m_samples[label];
}

bool Project::hasSample(const QString &label) const {
    return m_samples.contains(label);
}

const VoiceGroup *Project::getVoiceGroup(const QString &name) const {
    auto it = m_voicegroups.find(name);
    return it != m_voicegroups.end() ? &it.value() : nullptr;
}

void Project::addInstrumentToGroup(const QString &group, const Instrument &inst) {
    this->m_voicegroups[group].instruments.append(inst);
}

void Project::setVoiceGroupOffset(const QString &group, int offset) {
    this->m_voicegroups[group].offset = offset;
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

std::shared_ptr<Song> Project::addSong(const QString &title, smf::MidiFile &midi) {
    // guaranteed this function is only called when the song is not already in the song_table

    if (!this->m_song_entries.contains(title)) {
        // dont know how to load this song
        return nullptr;
    }

    SongEntry &entry = this->m_song_entries[title];

    // Constructing the song (which inherits MidiFile)
    //smf::MidiFile midi = this->m_i
    //qDebug() << "Project::loadSong" << title << entry.midifile;
    std::shared_ptr<Song> song = std::make_shared<Song>(midi);
    song->setMetaInfo(entry);

    this->m_song_table.insert(title, song);

    return song;
}
