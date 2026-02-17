#pragma once
#ifndef PROJECT_H
#define PROJECT_H

#include <QMap>
#include <memory>

#include "sound/song.h"
#include "sound/soundtypes.h"



class Project {
public:
    Project();
    ~Project();

public:
    void load();
    void load(QString path);

    std::shared_ptr<Song> addSong(const QString &title, smf::MidiFile &midi);
    std::shared_ptr<Song> getSong(const QString &title) { return this->m_song_table.contains(title) ? this->m_song_table[title] : nullptr; }

    std::shared_ptr<Song> activeSong() { return this->m_active_song; }
    void setActiveSong(const QString &title) { this->m_active_song = this->getSong(title); }
    void setRootPath(const QString &path) { m_root_path = path; }
    const QString &rootPath() const { return m_root_path; }

    // direct_sound_data.inc mappings
    void addSampleMapping(const QString &label, const QString &path);
    void addSample(const QString &label, const Sample &sample);
    void addPcmData(const QString &label, const QByteArray &data);
    void addInstrumentToGroup(const QString &group, const Instrument &inst);
    void setVoiceGroupOffset(const QString &group, int offset);
    void addTable(const QString &label, const KeysplitTable &table);
    void addSongEntry(const SongEntry &song);
    void updateSongData(const QString &title, const QString &voicegroup, int volume, int priority, int reverb);

public:
    int getNumSongsInTable() { return this->m_song_table_order.size(); }
    QString getSongTitleAt(int i) { return this->m_song_table_order.at(i); } // !TODO: bounds checking
    SongEntry &getSongEntryByTitle(const QString &title) { return this->m_song_entries[title]; }
    bool songLoaded(const QString &title) { return this->m_song_table.contains(title); }
    QString getSamplePath(const QString &label) const;
    Sample &getSample(const QString &label);

    bool hasSample(const QString &label) const;

    const QMap<QString, Sample> &getSamples() const { return m_samples; }
    const QMap<QString, QByteArray> &getPcmData() const { return m_pcm_data; }
    const QMap<QString, VoiceGroup> &getVoiceGroups() const { return m_voicegroups; }
    const QMap<QString, KeysplitTable> &getKeysplitTables() const { return m_keysplit_tables; }
    const VoiceGroup *getVoiceGroup(const QString &name) const;

private:
    std::shared_ptr<Song> m_active_song;

    QMap<QString, QString> m_sample_map; // label -> path
    QMap<QString, Sample> m_samples; // label -> loaded PCM data
    QMap<QString, QByteArray> m_pcm_data; // label -> 16-byte programmable wave data
    QMap<QString, VoiceGroup> m_voicegroups;
    QMap<QString, KeysplitTable> m_keysplit_tables;
    QMap<QString, SongEntry> m_song_entries; // song titles and filenames
    QStringList m_song_table_order; // just titles in order read !TODO: rename? (m_song_titles?)
    QMap<QString, std::shared_ptr<Song>> m_song_table; // loaded songs
    QString m_root_path;

    friend class ProjectInterface;
};

#endif // PROJECT_H
