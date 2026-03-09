#include "app/projectinterface.h"

#include "app/project.h"
#include "sound/soundtypes.h"
#include "util/logging.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <sstream>



const QString ProjectInterface::ProjectPath::direct_sound_data =
    "sound/direct_sound_data.inc";

const QString ProjectInterface::ProjectPath::programmable_wave_data =
    "sound/programmable_wave_data.inc";

const QString ProjectInterface::ProjectPath::voice_group_index =
    "sound/voice_groups.inc";



ProjectInterface::ProjectInterface(Project *project) : m_project(project) { }

void ProjectInterface::setRoot(const QString &root) {
    m_root = root;
}

QString ProjectInterface::concatPaths(const QString &p1, const QString &p2) {
    return QDir::cleanPath(QString("%1/%2").arg(p1).arg(p2));
}

QString ProjectInterface::readTextFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return QString();
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QString text;
    while (!in.atEnd()) {
        text += in.readLine() + '\n';
    }

    return text;
}

QByteArray ProjectInterface::readBinaryFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return QByteArray();
    }

    return file.readAll();
}

/**
 * Load all music data from the project root.
 */
bool ProjectInterface::loadProject(const QString &root) {
    QDir project_dir(root);

    if (!project_dir.exists("sound") || !project_dir.exists("include")) {
        logging::debug(QString("Project root not a valid decomp project. (%1)").arg(root),
                       logging::LogCategory::Project);
        return false;
    }

    m_project->reset();
    this->setRoot(root);

    return this->loadDirectSoundData()
        && this->loadProgrammableWaveData()
        && this->loadSamples()
        && this->loadVoiceGroups()
        && this->loadKeysplitTables()
        && this->loadSongs();
}

bool ProjectInterface::loadDirectSoundData() {
    QString path = this->concatPaths(m_root, ProjectPath::direct_sound_data);
    QString text = this->readTextFile(path);

    // match the sample label and incbin path together
    static const QRegularExpression regex_label_path(
        "(?<label>[A-Za-z0-9_]+):{1,2}[\\s\\S]*?\\.incbin\\s+\"(?<path>[A-Za-z0-9_./\\\\ ]+)\""
    );

    QRegularExpressionMatchIterator iter = regex_label_path.globalMatch(text);
    if (!iter.hasNext()) return false;

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString label = match.captured("label");
        QString resource_path = this->resolveResourcePath(match.captured("path"), ".wav");
        m_project->addSampleMapping(label, resource_path);
    }

    return true;
}

bool ProjectInterface::loadProgrammableWaveData() {
    QString path = this->concatPaths(m_root, ProjectPath::programmable_wave_data);
    QString text = this->readTextFile(path);
    if (text.isEmpty()) return false;

    static const QRegularExpression regex_label_path(
        "(?<label>[A-Za-z0-9_]+):{1,2}\\s+\\.incbin\\s+\"(?<path>[^\"]+)\"");

    QRegularExpressionMatchIterator iter = regex_label_path.globalMatch(text);
    if (!iter.hasNext()) return false;

    int loaded = 0;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString label = match.captured("label");
        QString pcm_path = this->concatPaths(m_root, match.captured("path"));

        QByteArray data = this->readBinaryFile(pcm_path);
        if (data.size() == 0x10) {
            m_project->addPcmData(label, data);
            loaded++;
        }
    }

    return loaded > 0;
}

bool ProjectInterface::loadSamples() {
    QMapIterator<QString, QString> iter(m_project->m_sample_map);
    int loaded = 0;

    while (iter.hasNext()) {
        iter.next();
        if (this->loadWavFile(iter.key(), iter.value())) {
            loaded++;
        }
    }

    return loaded > 0;
}

bool ProjectInterface::loadKeysplitTables() {
    QString text = this->readTextFile(this->concatPaths(m_root, "sound/keysplit_tables.inc"));
    if (text.isEmpty()) return false;

    QRegularExpression re_keysplit("keysplit\\s+(?<name>[A-Za-z0-9_]+),\\s+(?<offset>\\d+)");
    QRegularExpressionMatchIterator iter = re_keysplit.globalMatch(text);
    if (!iter.hasNext()) return false;

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured("name");
        int offset = match.captured("offset").toInt();

        // parse the nested keysplit voicegroup first
        this->parseKeysplitInstrument(name);

        KeysplitTable table;
        table.label = "keysplit_" + name;
        table.offset = offset;

        // read just the current keysplit block
        int start = match.capturedEnd();
        int end = text.indexOf("keysplit", start);
        if (end == -1) end = text.length();

        QString single_inst_text = text.mid(start, end - start);
        QRegularExpression split("split\\s+(?<i>\\d+),\\s+(?<max>\\d+)");
        QRegularExpressionMatchIterator iter_split = split.globalMatch(single_inst_text);

        int note = offset;
        while (iter_split.hasNext()) {
            QRegularExpressionMatch sm = iter_split.next();
            int i = sm.captured("i").toInt();
            int max = sm.captured("max").toInt();

            // fill the map with the instrument index for this range
            for (int k = note; k <= max; ++k) {
                table.note_map[k] = i;
            }

            note = max + 1;
        }

        m_project->addTable(table.label, table);
    }

    return true;
}

/**
 * Load the full song table and song config metadata.
 */
bool ProjectInterface::loadSongs() {
    QString text = this->readTextFile(this->concatPaths(m_root, "sound/song_table.inc"));
    if (text.isEmpty()) return false;

    QRegularExpression regex(
        "song\\s+(?<title>[A-Za-z0-9_]+),\\s+(?<player>[A-Za-z0-9_]+),\\s+(?<type_id>[x0-9A-Fa-f]+)"
    );
    QRegularExpressionMatchIterator iter = regex.globalMatch(text);

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();

        SongEntry song;
        song.title = match.captured("title");
        song.player = match.captured("player");

        QString type_id = match.captured("type_id");
        song.type = type_id.toInt(nullptr, 0);

        m_project->addSongEntry(song);
    }

    this->parseMidiConfig();
    this->validateMidiFiles();

    return true;
}

/**
 * Checks each song for a valid midi file, and invalidates the song if none exists.
 */
void ProjectInterface::validateMidiFiles() {
    if (!m_project) return;

    for (auto it = m_project->m_song_entries.begin(); it != m_project->m_song_entries.end(); it++) {
        SongEntry &entry = it.value();
        if (entry.midifile.isEmpty()) continue;

        QString path = this->concatPaths(m_root, "sound/songs/midi/" + entry.midifile);
        if (!QFile::exists(path)) {
            entry.midifile.clear();
        }
    }
}

bool ProjectInterface::loadWavFile(const QString &label, const QString &path) {
    QString error;
    QByteArray data = this->readBinaryFile(path, &error);

    if (data.isEmpty()) {
        logging::warn(QString("Failed to open wav file: %1 (%2)").arg(path, error),
                      logging::LogCategory::Audio);
        return false;
    }

    if (data.size() < 44) {
        logging::warn(QString("WAV file too small: %1").arg(path), logging::LogCategory::Audio);
        return false;
    }

    // verify riff header
    if (data.mid(0, 4) != "RIFF" || data.mid(8, 4) != "WAVE") {
        logging::warn(QString("Invalid WAV header: %1").arg(path), logging::LogCategory::Audio);
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    Sample sample;

    // parse chunks after the 12-byte riff header
    int pos = 12;
    while (pos + 8 <= data.size()) {
        QByteArray chunk_id = data.mid(pos, 4);

        stream.device()->seek(pos + 4);
        uint32_t chunk_size;
        stream >> chunk_size;

        if (chunk_id == "fmt ") {
            if (chunk_size >= 16) {
                stream.device()->seek(pos + 12);
                stream >> sample.sample_rate;
            }
        }
        else if (chunk_id == "smpl") {
            if (chunk_size >= 36) {
                stream.device()->seek(pos + 8 + 28);
                uint32_t num_loops;
                stream >> num_loops;

                if (num_loops > 0 && chunk_size >= 60) {
                    stream.device()->seek(pos + 8 + 36 + 8);
                    stream >> sample.loop_start >> sample.loop_end;
                    sample.loops = true;
                }
            }
        }
        else if (chunk_id == "data") {
            // wav 8-bit pcm is unsigned, convert it for playback
            QByteArray raw = data.mid(pos + 8, chunk_size);
            sample.data.resize(raw.size());

            for (int i = 0; i < raw.size(); i++) {
                sample.data[i] = static_cast<char>(static_cast<uint8_t>(raw[i]) - 128);
            }
        }

        pos += 8 + chunk_size;
        if (chunk_size % 2 == 1) pos++;
    }

    if (sample.data.isEmpty()) {
        logging::warn(QString("No data chunk found in WAV: %1").arg(path),
                      logging::LogCategory::Audio);
        return false;
    }

    m_project->addSample(label, sample);
    return true;
}

bool ProjectInterface::loadVoiceGroups() {
    QString index_path = this->concatPaths(m_root, ProjectPath::voice_group_index);
    QString text = this->readTextFile(index_path);
    if (text.isEmpty()) return false;

    QRegularExpression group_regex("\\.include\\s+\"(?<path>[^\"]+)\"");
    QRegularExpressionMatchIterator iter = group_regex.globalMatch(text);
    if (!iter.hasNext()) return false;

    while (iter.hasNext()) {
        QRegularExpressionMatch match_data = iter.next();
        QString include_path = match_data.captured("path");
        this->parseVoiceGroup(include_path);
    }

    return true;
}

bool ProjectInterface::parseVoiceGroup(const QString &path) {
    QString text = this->readTextFile(this->concatPaths(m_root, path));
    if (text.isEmpty()) {
        logging::warn(QString("Voicegroup file is empty: %1").arg(path),
                      logging::LogCategory::Project);
        return false;
    }

    // support both assembly labels and voice_group macro labels
    QRegularExpression label_regex(
        "(?:(?:^|\\n)\\s*(?<label>voicegroup\\d+)::)|"
        "(?:voice_group\\s+(?<label2>[A-Za-z0-9_]+)(?:,\\s*(?<offset>\\d+))?)");
    QRegularExpressionMatch label_match = label_regex.match(text);

    if (!label_match.hasMatch()) {
        logging::warn(QString("No voicegroup label found in: %1").arg(path),
                      logging::LogCategory::Project);
        return false;
    }

    QString group_label = label_match.captured("label");
    if (group_label.isEmpty()) {
        group_label = label_match.captured("label2");
    }

    QString offset_str = label_match.captured("offset");
    if (!offset_str.isEmpty()) {
        m_project->setVoiceGroupOffset(group_label, offset_str.toInt());
    }

    // parse each voice_* macro line in the file
    QRegularExpression line_regex(
        "^\\s*(?<type>voice_[A-Za-z0-9_]+)\\s+(?<args>[^\\n]+)",
        QRegularExpression::MultilineOption);
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

/**
 * Parse one voice macro into an instrument entry.
 */
void ProjectInterface::processInstrumentEntry(const QString &group_label,
                                              const QString &type,
                                              const QStringList &args) {
    Instrument inst;
    inst.type = type;

    // directsound voices
    if (type.startsWith("voice_directsound") && args.size() >= 7) {
        if (type == "voice_directsound") inst.type_id = 0x00;
        else if (type == "voice_directsound_no_resample") inst.type_id = 0x08;
        else if (type == "voice_directsound_alt") inst.type_id = 0x10;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.sample_label = args.at(2);
        inst.attack = args.at(3).toInt();
        inst.decay = args.at(4).toInt();
        inst.sustain = args.at(5).toInt();
        inst.release = args.at(6).toInt();
    }

    // square 1 voices
    else if (type.startsWith("voice_square_1") && args.size() >= 8) {
        if (type.endsWith("alt")) inst.type_id = 0x09;
        else inst.type_id = 0x01;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.sweep = args.at(2).toInt();
        inst.duty_cycle = args.at(3).toInt();
        inst.attack = args.at(4).toInt();
        inst.decay = args.at(5).toInt();
        inst.sustain = args.at(6).toInt();
        inst.release = args.at(7).toInt();
    }

    // square 2 voices
    else if (type.startsWith("voice_square_2") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 0x0A;
        else inst.type_id = 0x02;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.duty_cycle = args.at(2).toInt();
        inst.attack = args.at(3).toInt();
        inst.decay = args.at(4).toInt();
        inst.sustain = args.at(5).toInt();
        inst.release = args.at(6).toInt();
    }

    // programmable wave voices
    else if (type.startsWith("voice_programmable_wave") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 0x0B;
        else inst.type_id = 0x03;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.sample_label = args.at(2);
        inst.attack = args.at(3).toInt();
        inst.decay = args.at(4).toInt();
        inst.sustain = args.at(5).toInt();
        inst.release = args.at(6).toInt();
    }

    // noise voices
    else if (type.startsWith("voice_noise") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 0x0C;
        else inst.type_id = 0x04;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.duty_cycle = args.at(2).toInt();
        inst.attack = args.at(3).toInt();
        inst.decay = args.at(4).toInt();
        inst.sustain = args.at(5).toInt();
        inst.release = args.at(6).toInt();
    }

    // note-based keysplit wrapper
    else if (type == "voice_keysplit" && args.size() >= 2) {
        QString name = args.at(0);
        if (name.startsWith("voicegroup_")) {
            name = name.mid(11);
        }

        inst.type_id = 0x40;
        inst.sample_label = name;
        inst.keysplit_table = args.at(1);
    }

    // drumset-style keysplit wrapper
    else if (type == "voice_keysplit_all") {
        QString name = args.at(0);
        if (name.startsWith("voicegroup_")) {
            name = name.mid(11);
        }

        inst.type_id = 0x80;
        inst.sample_label = name;
        inst.base_key = g_midi_middle_c;
        inst.pan = 0x00;
    }

    m_project->addInstrumentToGroup(group_label, inst);
}

/**
 * Parse the nested voicegroup used by one keysplit table.
 */
void ProjectInterface::parseKeysplitInstrument(const QString &name) {
    QString text = this->readTextFile(
        this->concatPaths(m_root, "sound/voicegroups/keysplits/" + name + ".inc"));
    if (text.isEmpty()) return;

    QRegularExpression re_label("voice_group\\s+(?<label>[A-Za-z0-9_]+)");
    QRegularExpressionMatch match = re_label.match(text);
    if (!match.hasMatch()) return;

    QString group_label = match.captured("label");

    QRegularExpression inst("(?<type>voice_[a-z0-9_]+)\\s+(?<args>[^v]+)");
    QRegularExpressionMatchIterator iter = inst.globalMatch(text);

    while (iter.hasNext()) {
        QRegularExpressionMatch match_line = iter.next();
        QString type = match_line.captured("type");
        if (type == "voice_group") continue;

        QString raw = match_line.captured("args").simplified();
        QStringList args = raw.split(QRegularExpression(",\\s*"));

        this->processInstrumentEntry(group_label, type, args);
    }
}

/**
 * Parse song metadata from midi.cfg, where info like volume and voicegroup are stored.
 */
void ProjectInterface::parseMidiConfig() {
    QString text = this->readTextFile(this->concatPaths(m_root, "sound/songs/midi/midi.cfg"));
    if (text.isEmpty()) {
        logging::warn("midi.cfg not found", logging::LogCategory::Project);
        return;
    }

    static const QRegularExpression re_title("(?<title>[A-Za-z0-9_]+)\\.mid:");
    static const QRegularExpression re_voicegroup("-G(?<voicegroup>[A-Za-z0-9_]+)");
    static const QRegularExpression re_volume("-V(?<volume>\\d+)");
    static const QRegularExpression re_priority("-P(?<priority>\\d+)");
    static const QRegularExpression re_reverb("-R(?<reverb>\\d+)");

    QStringList lines = text.split('\n');

    int parsed = 0;
    for (const QString &line : lines) {
        QRegularExpressionMatch title_match = re_title.match(line);
        if (!title_match.hasMatch()) continue;

        QString title = title_match.captured("title");

        QString voicegroup = "dummy";
        int volume = 0x7F;
        int priority = 0;
        int reverb = 0;

        QRegularExpressionMatch group_match = re_voicegroup.match(line);
        if (group_match.hasMatch()) {
            voicegroup = group_match.captured("voicegroup");
            if (voicegroup.startsWith("_")) {
                voicegroup = voicegroup.mid(1);
            }
        }

        QRegularExpressionMatch volume_match = re_volume.match(line);
        if (volume_match.hasMatch()) {
            volume = volume_match.captured("volume").toInt();
        }

        QRegularExpressionMatch priority_match = re_priority.match(line);
        if (priority_match.hasMatch()) {
            priority = priority_match.captured("priority").toInt();
        }

        QRegularExpressionMatch reverb_match = re_reverb.match(line);
        if (reverb_match.hasMatch()) {
            reverb = reverb_match.captured("reverb").toInt();
        }

        m_project->updateSongData(title, voicegroup, volume, priority, reverb);
        parsed++;
    }
}

/**
 * Load one midi file by song title.
 */
smf::MidiFile ProjectInterface::loadMidi(const QString &title) {
    smf::MidiFile midifile;

    QString path = this->concatPaths(m_root, "sound/songs/midi/" + title + ".mid");

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return midifile;
    }

    std::istringstream is(f.readAll().toStdString());
    midifile.read(is);

    return midifile;
}

/**
 * Resolve a resource path and preferred extension inside the project root.
 */
QString ProjectInterface::resolveResourcePath(const QString &original_path,
                                              const QString &preferred_ext) {
    QString source_path = original_path;

    int last_dot = source_path.lastIndexOf('.');
    if (last_dot != -1) {
        source_path.replace(last_dot, source_path.length() - last_dot, preferred_ext);
    }

    QString absolute_path = this->concatPaths(m_root, source_path);
    if (QFile::exists(absolute_path)) {
        return absolute_path;
    }

    return this->concatPaths(m_root, original_path);
}
