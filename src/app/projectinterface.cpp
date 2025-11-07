#include "projectinterface.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>

#include "project.h"
#include "soundtypes.h"



const QString ProjectInterface::ProjectPath::direct_sound_data = "sound/direct_sound_data.inc";
const QString ProjectInterface::ProjectPath::voice_group_index = "sound/voice_groups.inc";


ProjectInterface::ProjectInterface(Project *project) : m_project(project) {
    //
}

bool ProjectInterface::loadProject(const QString &root) {
    QDir project_dir(root);

    if (!project_dir.exists("sound") || !project_dir.exists("include")) {
        return false;
    }

    this->m_project->load(); // TODO: remove this later?

    this->setRoot(root);

    return this->loadDirectSoundData()
        && this->loadVoiceGroups();
}

QString ProjectInterface::concatPaths(QString p1, QString p2) {
    return QDir::cleanPath(QString("%1/%2").arg(p1).arg(p2));
}

QString ProjectInterface::resolveResourcePath(const QString &original_path, const QString &preferred_ext) {
    QString source_path = original_path;

    // Find the extension and replace it
    int last_dot = source_path.lastIndexOf('.');
    if (last_dot != -1) {
        source_path.replace(last_dot, source_path.length() - last_dot, preferred_ext);
    }

    QString absolute_path = this->concatPaths(this->m_root, source_path);

    // Make sure the source file exists, otherwise use original
    if (QFile::exists(absolute_path)) {
        return absolute_path;
    }
    return this->concatPaths(this->m_root, original_path);
}

// stole from porymap
QString ProjectInterface::readTextFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return QString();
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString text = "";
    while (!in.atEnd()) {
        text += in.readLine() + '\n';
    }

    return text;
}

bool ProjectInterface::loadDirectSoundData() {
    QString path = this->concatPaths(this->m_root, ProjectPath::direct_sound_data);

    QString text = this->readTextFile(path);

    static const QRegularExpression regex_label_path("(?<label>[A-Za-z0-9_]+):{1,2}\\s+\\.incbin\\s+\"(?<path>[A-Za-z0-9_./\\\\ ]+)\"");

    QRegularExpressionMatchIterator iter = regex_label_path.globalMatch(text);
    
    if (!iter.hasNext()) return false; // no matches is an unsuccessful load

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString label = match.captured("label");
        QString resource_path = this->resolveResourcePath(match.captured("path"), ".wav");
        this->m_project->addSampleMapping(label, resource_path);
    }

    return true;
}

bool ProjectInterface::loadVoiceGroups() {
    // The index is the file which lists all available voice groups
    QString index_path = this->concatPaths(this->m_root, ProjectPath::voice_group_index);
    
    QString text = this->readTextFile(index_path);
    if (text.isEmpty()) {
        return false;
    }

    QRegularExpression group_regex("\\.include\\s+\"(?<path>[^\"]+)\"");

    QRegularExpressionMatchIterator iter = group_regex.globalMatch(text);
    
    if (!iter.hasNext()) return false; // no matches is an unsuccessful load

    while (iter.hasNext()) {
        QRegularExpressionMatch match_data = iter.next();
        QString include_path = match_data.captured("path");

        // read the individual voice group file
        this->parseVoiceGroup(include_path);
    }

    return true;
}

bool ProjectInterface::parseVoiceGroup(const QString &path) {
    QString text = this->readTextFile(this->concatPaths(this->m_root, path));
    if (text.isEmpty()) {
        return false;
    }

    // only need to extract label once (first line)
    QRegularExpression label_regex("voice_group\\s+(?<label>[A-Za-z0-9_]+)");
    QRegularExpressionMatch label_match = label_regex.match(text);
    
    if (!label_match.hasMatch()) {
        return false;
    }
    QString group_label = label_match.captured("label");

    // the rest of the lines are <type macro> <args...>
    // the type macro is prefixed with voice_
    QRegularExpression line_regex("(?<type>voice_[A-Za-z0-9_]+)\\s+(?<args>[^v]+)");
    QRegularExpressionMatchIterator iter = line_regex.globalMatch(text);

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString instrument_type = match.captured("type");
        
        if (instrument_type == "voice_group") continue;

        QString arg_string = match.captured("args").simplified();
        QStringList args = arg_string.split(QRegularExpression(",\\s*"));

        this->processInstrumentEntry(group_label, instrument_type, args);
    }

    return true;
}

void ProjectInterface::processInstrumentEntry(const QString &group_label, const QString &type, const QStringList &args) {
    Instrument inst;
    inst.type = type;

    // .macro voice_directsound base_midi_key:req, pan:req, sample_data_pointer:req, attack:req, decay:req, sustain:req, release:req
    if (type.startsWith("voice_directsound") && args.size() >= 7) {
        if (type == "voice_directsound") inst.type_id = 0x00;
        else if (type == "voice_directsound_no_resample") inst.type_id = 0x08;
        else if (type == "voice_directsound_alt") inst.type_id = 0x10;

        inst.base_key     = args.at(0).toInt();
        inst.pan          = args.at(1).toInt();
        inst.sample_label = args.at(2);
        inst.attack       = args.at(3).toInt();
        inst.decay        = args.at(4).toInt();
        inst.sustain      = args.at(5).toInt();
        inst.release      = args.at(6).toInt();
    }

    // voice_square_1 base_midi_key:req, pan:req, sweep:req, duty_cycle:req, attack:req, decay:req, sustain:req, release:req
    else if (type.startsWith("voice_square_1") && args.size() >= 8) {
        if (type.endsWith("alt")) inst.type_id = 0x09;
        else inst.type_id = 0x01;

        inst.base_key     = args.at(0).toInt();
        inst.pan          = args.at(1).toInt();
        inst.sweep        = args.at(2).toInt();
        inst.duty_cycle   = args.at(3).toInt();
        inst.attack       = args.at(4).toInt();
        inst.decay        = args.at(5).toInt();
        inst.sustain      = args.at(6).toInt();
        inst.release      = args.at(7).toInt();
    }

    // voice_programmable_wave base_midi_key:req, pan:req, wave_samples_pointer:req, attack:req, decay:req, sustain:req, release:req
    else if (type.startsWith("voice_programmable_wave") && args.size() >= 7) {
        inst.base_key     = args.at(0).toInt();
        inst.pan          = args.at(1).toInt();
        inst.sample_label = args.at(2); // Label for the 32-byte wave data
        inst.attack       = args.at(3).toInt();
        inst.decay        = args.at(4).toInt();
        inst.sustain      = args.at(5).toInt();
        inst.release      = args.at(6).toInt();
    }

    // voice_keysplit voice_group_pointer:req, keysplit_table_pointer:req
    else if (type == "voice_keysplit" && args.size() >= 2) {
        inst.type_id = 0x40;
        QString target_group = args.at(0);
        QString table_label = args.at(1);

        this->m_project->addKeysplitInstrument(group_label, target_group, table_label);
    }
    
    // voice_keysplit_all voice_group_pointer:req
    else if (type == "voice_keysplit_all" && args.size() >= 1) {
        inst.type_id = 0x80;
        QString target_group = args.at(0); // voice_group_pointer

        this->m_project->addKeysplitInstrument(group_label, target_group, "all");
    }

    // pass the completed instrument to the project
    this->m_project->addInstrumentToGroup(group_label, inst);
}

