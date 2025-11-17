#pragma once
#ifndef PROJECTINTERFACE_H
#define PROJECTINTERFACE_H

#include <QString>
#include <QDir>
#include <memory>

#include "MidiFile.h"
#include "soundtypes.h"

class Project;

class ProjectInterface {
public:
    ProjectInterface(Project *project);

    void setRoot(const QString &root) { this->m_root = root; }
    QString concatPaths(QString p1, QString p2);

    QString readTextFile(const QString &path, QString *error = nullptr);
    QByteArray readBinaryFile(const QString &path, QString *error = nullptr);

    bool loadProject(const QString &root);

    bool loadDirectSoundData();
    bool loadProgrammableWaveData();
    bool loadSamples();
    bool loadWavFile(const QString &label, const QString &path);
    bool loadVoiceGroups();
    bool parseVoiceGroup(const QString &path);
    bool loadKeysplitTables();
    bool loadSongs();

    void processInstrumentEntry(const QString &group_label, const QString &type, const QStringList &args);
    void parseKeysplitInstrument(const QString &name);
    void parseMidiConfig();

    smf::MidiFile loadMidi(const QString &title);

    // bool saveProject();
    // bool saveVoicegroup(int group_id);
    // bool saveSongTable();

private:
    QString resolveResourcePath(const QString &raw_path, const QString &preferred_ext);

private:
    // void parseSongTable(const QString &path);
    // void parseVoicegroups(const QDir &voice_dir);
    // void parseSampleData(const QString &path);

    Project *m_project = nullptr;
    QString m_root;

private:
    // maybe a silly way to define path constants
    struct ProjectPath {
        static const QString direct_sound_data;
        static const QString programmable_wave_data;
        static const QString voice_group_index;
    };
};

#endif // PROJECTINTERFACE_H
