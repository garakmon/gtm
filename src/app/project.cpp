#include "app/project.h"

#include <QDebug>



/**
 * Reset the project state before loading a new project.
 * 
 * !TODO: just delete and replace old projects?
 */
void Project::reset() {
    m_song_table.clear();
    m_active_song.reset();

    m_sample_map.clear();
    m_samples.clear();
    m_pcm_data.clear();
    m_voicegroups.clear();
    m_keysplit_tables.clear();

    m_song_entries.clear();
    m_song_table_order.clear();
}

void Project::setRootPath(const QString &path) {
    m_root = path;
}

const QString &Project::rootPath() const {
    return m_root;
}

/**
 * Create and stores a loaded song object from a midi file.
 */
std::shared_ptr<Song> Project::addSong(const QString &title, smf::MidiFile &midi) {
    if (!m_song_entries.contains(title)) return nullptr;

    SongEntry &entry = m_song_entries[title];

    std::shared_ptr<Song> song = std::make_shared<Song>(midi);
    song->setMetaInfo(entry);

    m_song_table.insert(title, song);

    return song;
}

std::shared_ptr<Song> Project::getSong(const QString &title) const {
    return m_song_table.contains(title) ? m_song_table.value(title) : nullptr;
}

std::shared_ptr<Song> Project::activeSong() const {
    return m_active_song;
}

void Project::setActiveSong(const QString &title) {
    m_active_song = this->getSong(title);
}

/**
 * Store the source path for a direct sound sample label.
 */
void Project::addSampleMapping(const QString &label, const QString &path) {
    m_sample_map.insert(label, path);
}

/**
 * Store decoded sample data for a label.
 */
void Project::addSample(const QString &label, const Sample &sample) {
    m_samples.insert(label, sample);
}

/**
 * Store programmable wave data for a label.
 */
void Project::addPcmData(const QString &label, const QByteArray &data) {
    m_pcm_data.insert(label, data);
}

/**
 * Append an instrument to the voicegroup.
 */
void Project::addInstrumentToGroup(const QString &group, const Instrument &inst) {
    m_voicegroups[group].instruments.append(inst);
}

void Project::setVoiceGroupOffset(const QString &group, int offset) {
    m_voicegroups[group].offset = offset;
}

/**
 * Store a keysplit table.
 */
void Project::addTable(const QString &label, const KeysplitTable &table) {
    m_keysplit_tables[label] = table;
}

void Project::addSongEntry(const SongEntry &song) {
    m_song_table_order.append(song.title);
    m_song_entries[song.title] = song;
}

/**
 * Update song data from the parsed build config.
 */
void Project::updateSongData(const QString &title, const QString &voicegroup,
                             int volume, int priority, int reverb) {
    if (!m_song_entries.contains(title)) return;

    SongEntry &song = m_song_entries[title];

    song.voicegroup = voicegroup;
    song.volume = volume;
    song.priority = priority;
    song.reverb = reverb;
    song.midifile = title + ".mid"; // !TODO: should we really enforce <title>.mid
}

int Project::getNumSongsInTable() const {
    return m_song_table_order.size();
}

QString Project::getSongTitleAt(int i) const {
    return m_song_table_order.at(i);
}

/**
 * Get mutable song entry data for a title.
 */
SongEntry &Project::getSongEntryByTitle(const QString &title) {
    return m_song_entries[title];
}

/**
 * Get whether a song has been loaded in the song table, meaning the midi file.
 */
bool Project::songLoaded(const QString &title) const {
    return m_song_table.contains(title);
}

QString Project::getSamplePath(const QString &label) const {
    return m_sample_map.value(label);
}

Sample &Project::getSample(const QString &label) {
    return m_samples[label];
}

bool Project::hasSample(const QString &label) const {
    return m_samples.contains(label);
}

const QMap<QString, Sample> &Project::getSamples() const {
    return m_samples;
}

const QMap<QString, QByteArray> &Project::getPcmData() const {
    return m_pcm_data;
}

const QMap<QString, VoiceGroup> &Project::getVoiceGroups() const {
    return m_voicegroups;
}

const QMap<QString, KeysplitTable> &Project::getKeysplitTables() const {
    return m_keysplit_tables;
}

const VoiceGroup *Project::getVoiceGroup(const QString &name) const {
    auto it = m_voicegroups.find(name);
    return it != m_voicegroups.end() ? &it.value() : nullptr;
}
