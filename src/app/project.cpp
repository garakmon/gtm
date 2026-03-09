#include "app/project.h"

#include <QRegularExpression>



static const QRegularExpression s_song_title_regex("mus_[a-z0-9_]+");
const Project::DefaultSongSettings Project::s_default_song_settings{};



bool Project::createSong(const NewSongSettings &settings, QString *error) {
    if (!this->validateNewSongSettings(settings, error)) {
        return false;
    }

    SongEntry entry = this->buildSongEntryFromSettings(settings);

    smf::MidiFile midi;
    midi.setTicksPerQuarterNote(s_default_song_settings.tpqn);
    const VoiceGroup *voicegroup = this->getVoiceGroup(entry.voicegroup);
    Song::addDefaultEvents(midi, voicegroup, s_default_song_settings.events);

    return this->insertNewSongData(entry, midi, error);
}

bool Project::hasUnsavedChanges() const {
    return m_has_unsaved_changes;
}

void Project::setUnsavedChanges(bool dirty) {
    m_has_unsaved_changes = dirty;
}

bool Project::isSongUnsaved(const QString &title) const {
    return m_unsaved_song_titles.contains(title);
}

void Project::setSongUnsaved(const QString &title, bool dirty) {
    if (dirty) {
        m_unsaved_song_titles.insert(title);
        m_has_unsaved_changes = true;
        return;
    }

    m_unsaved_song_titles.remove(title);
    if (m_unsaved_song_titles.isEmpty()) {
        m_has_unsaved_changes = false;
    }
}

bool Project::validateNewSongSettings(const NewSongSettings &settings, QString *error) const {
    const QString title = settings.title.trimmed();
    if (title.isEmpty()) {
        if (error) *error = "Song title is empty.";
        return false;
    }
    if (!s_song_title_regex.match(title).hasMatch()) {
        if (error) *error = "Song title must match mus_[a-z0-9_]+.";
        return false;
    }
    if (m_song_entries.contains(title)) {
        if (error) *error = QString("Song title '%1' already exists.").arg(title);
        return false;
    }

    const QString voicegroup = settings.voicegroup.trimmed();
    if (voicegroup.isEmpty()) {
        if (error) *error = "Voicegroup is empty.";
        return false;
    }
    if (!m_voicegroups.contains(voicegroup)) {
        if (error) *error = QString("Voicegroup '%1' does not exist.").arg(voicegroup);
        return false;
    }

    return true;
}

SongEntry Project::buildSongEntryFromSettings(const NewSongSettings &settings) const {
    SongEntry entry;
    entry.title = settings.title.trimmed();
    entry.voicegroup = settings.voicegroup.trimmed();
    entry.player = settings.player.trimmed();
    entry.type = 0;
    entry.volume = settings.volume;
    entry.priority = settings.priority;
    entry.reverb = settings.reverb;
    entry.midifile = entry.title + ".mid";
    return entry;
}

bool Project::insertNewSongData(const SongEntry &entry, const smf::MidiFile &midi, QString *error) {
    if (m_song_entries.contains(entry.title)) {
        if (error) *error = QString("Song title '%1' already exists.").arg(entry.title);
        return false;
    }

    this->addSongEntry(entry);

    smf::MidiFile midi_copy(midi);
    std::shared_ptr<Song> song = this->addSong(entry.title, midi_copy);
    if (!song) {
        this->removeSongInternal(entry.title);
        if (error) *error = QString("Failed to create in-memory song '%1'.").arg(entry.title);
        return false;
    }

    m_active_song = song;
    this->setSongUnsaved(entry.title, true);
    return true;
}

void Project::removeSongInternal(const QString &title) {
    auto loaded = m_song_table.find(title);
    if (loaded != m_song_table.end()) {
        if (m_active_song == loaded.value()) {
            m_active_song.reset();
        }
        m_song_table.erase(loaded);
    }

    m_song_entries.remove(title);
    m_song_table_order.removeAll(title);
    m_unsaved_song_titles.remove(title);
    if (m_unsaved_song_titles.isEmpty()) {
        m_has_unsaved_changes = false;
    }
}



/**
 * Reset the project state before loading a new project.
 * 
 * !TODO: just delete and replace old projects?
 */
void Project::reset() {
    m_song_table.clear();
    m_active_song.reset();
    m_has_unsaved_changes = false;
    m_unsaved_song_titles.clear();

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
