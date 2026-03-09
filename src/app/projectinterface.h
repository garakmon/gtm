#pragma once
#ifndef PROJECTINTERFACE_H
#define PROJECTINTERFACE_H

#include "deps/midifile/MidiFile.h"
#include "sound/soundtypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>



class Project;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// ProjectInterface is the file-system and parsing layer for a decomp project.
/// It reads the project from its root, parses the necessary resource files,
/// and populates the Project with the results.
///
//////////////////////////////////////////////////////////////////////////////////////////
class ProjectInterface {
public:
    ProjectInterface(Project *project);
    ~ProjectInterface() = default;

    ProjectInterface(const ProjectInterface &) = delete;
    ProjectInterface &operator=(const ProjectInterface &) = delete;
    ProjectInterface(ProjectInterface &&) = delete;
    ProjectInterface &operator=(ProjectInterface &&) = delete;

    void setRoot(const QString &root);
    QString concatPaths(const QString &p1, const QString &p2);

    // file io
    QString readTextFile(const QString &path, QString *error = nullptr);
    QByteArray readBinaryFile(const QString &path, QString *error = nullptr);

    // project loading
    bool loadProject(const QString &root);
    bool loadDirectSoundData();
    bool loadProgrammableWaveData();
    bool loadSamples();
    bool loadKeysplitTables();
    bool loadSongs();
    void validateMidiFiles();

    // parsing helpers
    bool loadWavFile(const QString &label, const QString &path);
    bool loadVoiceGroups();
    bool parseVoiceGroup(const QString &path);
    void processInstrumentEntry(const QString &group_label, const QString &type,
                                const QStringList &args);
    void parseKeysplitInstrument(const QString &name);
    void parseMidiConfig();

    // song loading
    smf::MidiFile loadMidi(const QString &title);

private:
    // internal helpers
    QString resolveResourcePath(const QString &raw_path, const QString &preferred_ext);

    // owned state
    Project *m_project = nullptr;
    QString m_root;

    // project path constants
    // !TODO: configable?
    struct ProjectPath {
        static const QString direct_sound_data;
        static const QString programmable_wave_data;
        static const QString voice_group_index;
    };
};

#endif // PROJECTINTERFACE_H
