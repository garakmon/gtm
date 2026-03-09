#pragma once
#ifndef PROJECT_H
#define PROJECT_H

#include "app/structs.h"
#include "sound/soundtypes.h"
#include "sound/song.h"

#include <QMap>
#include <QSet>

#include <memory>



//////////////////////////////////////////////////////////////////////////////////////////
///
/// The Project is the model of the currently opened decomp project and its music data.
/// It owns the relevant resources (sound samples, PCM wave data, voicegroups, keysplits),
/// and the set of Song objects that have actually been instantiated.
///
//////////////////////////////////////////////////////////////////////////////////////////
class Project {
public:
    Project() = default;
    ~Project() = default;

    Project(const Project &) = delete;
    Project &operator=(const Project &) = delete;
    Project(Project &&) = delete;
    Project &operator=(Project &&) = delete;

    void reset();
    void setRootPath(const QString &path);
    const QString &rootPath() const;

    // song data stuff
    bool createSong(const NewSongSettings &settings, QString *error = nullptr);
    std::shared_ptr<Song> addSong(const QString &title, smf::MidiFile &midi);
    std::shared_ptr<Song> getSong(const QString &title) const;
    std::shared_ptr<Song> activeSong() const;
    void setActiveSong(const QString &title);
    bool hasUnsavedChanges() const;
    void setUnsavedChanges(bool dirty);
    bool isSongUnsaved(const QString &title) const;
    void setSongUnsaved(const QString &title, bool dirty);

    // loading
    void addSampleMapping(const QString &label, const QString &path);
    void addSample(const QString &label, const Sample &sample);
    void addPcmData(const QString &label, const QByteArray &data);
    void addInstrumentToGroup(const QString &group, const Instrument &inst);
    void setVoiceGroupOffset(const QString &group, int offset);
    void addTable(const QString &label, const KeysplitTable &table);
    void addSongEntry(const SongEntry &song);
    void updateSongData(const QString &title, const QString &voicegroup,
                        int volume, int priority, int reverb);

    // getters
    int getNumSongsInTable() const;
    QString getSongTitleAt(int i) const;
    SongEntry &getSongEntryByTitle(const QString &title);
    bool songLoaded(const QString &title) const;
    QString getSamplePath(const QString &label) const;
    Sample &getSample(const QString &label);
    bool hasSample(const QString &label) const;
    const QMap<QString, Sample> &getSamples() const;
    const QMap<QString, QByteArray> &getPcmData() const;
    const QMap<QString, VoiceGroup> &getVoiceGroups() const;
    const QMap<QString, KeysplitTable> &getKeysplitTables() const;
    const VoiceGroup *getVoiceGroup(const QString &name) const;

private:
    struct DefaultSongSettings {
        int tpqn = 48;
        Song::DefaultEventSettings events;
    };
    static const DefaultSongSettings s_default_song_settings;

    bool validateNewSongSettings(const NewSongSettings &settings, QString *error) const;
    SongEntry buildSongEntryFromSettings(const NewSongSettings &settings) const;
    bool insertNewSongData(const SongEntry &entry, const smf::MidiFile &midi, QString *error);
    void removeSongInternal(const QString &title);

    // songs
    QMap<QString, std::shared_ptr<Song>> m_song_table;
    std::shared_ptr<Song> m_active_song;
    bool m_has_unsaved_changes = false;
    QSet<QString> m_unsaved_song_titles;

    // project resources
    QMap<QString, QString> m_sample_map; // label -> path
    QMap<QString, Sample> m_samples; // label -> loaded PCM data
    QMap<QString, QByteArray> m_pcm_data; // label -> 16-byte programmable wave data
    QMap<QString, VoiceGroup> m_voicegroups;
    QMap<QString, KeysplitTable> m_keysplit_tables;

    // song table data
    QMap<QString, SongEntry> m_song_entries; // song titles and filenames
    QStringList m_song_table_order;

    // project metadata
    QString m_root;

    friend class ProjectInterface;
};

#endif // PROJECT_H
