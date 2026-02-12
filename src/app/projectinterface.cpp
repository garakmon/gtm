#include "app/projectinterface.h"

#include <QFile>
#include <QDir>
#include <QDataStream>
#include <QTextStream>
#include <QRegularExpression>

#include "app/project.h"
#include "sound/soundtypes.h"



const QString ProjectInterface::ProjectPath::direct_sound_data = "sound/direct_sound_data.inc";
const QString ProjectInterface::ProjectPath::programmable_wave_data = "sound/programmable_wave_data.inc";
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
        && this->loadProgrammableWaveData()
        && this->loadSamples()
        && this->loadVoiceGroups()
        && this->loadKeysplitTables()
        && this->loadSongs();
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

QByteArray ProjectInterface::readBinaryFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return QByteArray();
    }
    return file.readAll();
}

bool ProjectInterface::loadDirectSoundData() {
    QString path = this->concatPaths(this->m_root, ProjectPath::direct_sound_data);

    QString text = this->readTextFile(path);

    // Match label followed by .incbin, allowing for comments and newlines between them
    // Handles both: "Label:: .incbin" (same line) and "Label:: @comment\n\t.incbin" (next line)
    // [\s\S]*? matches any character including newlines, non-greedily
    static const QRegularExpression regex_label_path(
        "(?<label>[A-Za-z0-9_]+):{1,2}[\\s\\S]*?\\.incbin\\s+\"(?<path>[A-Za-z0-9_./\\\\ ]+)\"");

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

bool ProjectInterface::loadProgrammableWaveData() {
    QString path = this->concatPaths(this->m_root, ProjectPath::programmable_wave_data);

    QString text = this->readTextFile(path);
    if (text.isEmpty()) {
        return false;
    }

    static const QRegularExpression regex_label_path("(?<label>[A-Za-z0-9_]+):{1,2}\\s+\\.incbin\\s+\"(?<path>[^\"]+)\"");

    QRegularExpressionMatchIterator iter = regex_label_path.globalMatch(text);

    if (!iter.hasNext()) return false;

    int loaded = 0;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString label = match.captured("label");
        QString pcm_path = this->concatPaths(this->m_root, match.captured("path"));

        QByteArray data = this->readBinaryFile(pcm_path);
        if (data.size() == 0x10) {
            this->m_project->addPcmData(label, data);
            loaded++;
        }
    }

    qDebug() << "loaded" << loaded << "programmable wave samples";
    return loaded > 0;
}

bool ProjectInterface::loadSamples() {
    // iterate over every sample mapping and load the wav file
    QMapIterator<QString, QString> iter(m_project->m_sample_map);
    int loaded = 0;

    while (iter.hasNext()) {
        iter.next();
        if (loadWavFile(iter.key(), iter.value())) {
            loaded++;
        }
    }

    qDebug() << "loaded" << loaded << "samples";
    return loaded > 0;
}

bool ProjectInterface::loadWavFile(const QString &label, const QString &path) {
    QString error;
    QByteArray data = readBinaryFile(path, &error);

    if (data.isEmpty()) {
        qDebug() << "failed to open wav file:" << path << error;
        return false;
    }

    if (data.size() < 44) {
        qDebug() << "wav file too small:" << path;
        return false;
    }

    // verify RIFF header
    if (data.mid(0, 4) != "RIFF" || data.mid(8, 4) != "WAVE") {
        qDebug() << "invalid wav header:" << path;
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    Sample sample;

    // parse chunks (skip 12-byte RIFF header)
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
            // WAV 8-bit PCM is unsigned (0-255, 128=silence)
            // Convert to signed (-128 to 127, 0=silence) for playback
            QByteArray raw = data.mid(pos + 8, chunk_size);
            sample.data.resize(raw.size());
            for (int i = 0; i < raw.size(); i++) {
                sample.data[i] = static_cast<char>(static_cast<uint8_t>(raw[i]) - 128);
            }
        }

        pos += 8 + chunk_size;
        if (chunk_size % 2 == 1) pos++; // padding byte
    }

    if (sample.data.isEmpty()) {
        qDebug() << "no data chunk found in wav:" << path;
        return false;
    }

    m_project->addSample(label, sample);
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
        qDebug() << "parseVoiceGroup: empty file:" << path;
        return false;
    }

    // extract label - support both formats:
    // 1. "voicegroupXXX::" (assembly label with double colon)
    // 2. "voice_group <label>[, <offset>]" (macro format with optional offset for drumsets)
    QRegularExpression label_regex("(?:(?:^|\\n)\\s*(?<label>voicegroup\\d+)::)|(?:voice_group\\s+(?<label2>[A-Za-z0-9_]+)(?:,\\s*(?<offset>\\d+))?)");
    QRegularExpressionMatch label_match = label_regex.match(text);

    if (!label_match.hasMatch()) {
        qDebug() << "parseVoiceGroup: no voicegroup label in:" << path;
        return false;
    }

    QString group_label = label_match.captured("label");
    if (group_label.isEmpty()) {
        group_label = label_match.captured("label2");
    }

    // set offset if present (drumsets have starting MIDI note)
    QString offset_str = label_match.captured("offset");
    if (!offset_str.isEmpty()) {
        this->m_project->setVoiceGroupOffset(group_label, offset_str.toInt());
    }

    qDebug() << "parseVoiceGroup: loaded" << group_label << "from" << path
             << (offset_str.isEmpty() ? "" : QString(" (offset: %1)").arg(offset_str));

    // the rest of the lines are <type macro> <args...>
    // the type macro is prefixed with voice_
    QRegularExpression line_regex("^\\s*(?<type>voice_[A-Za-z0-9_]+)\\s+(?<args>[^\\n]+)", QRegularExpression::MultilineOption);
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

    // voice_square_2 base_midi_key:req, pan:req, duty_cycle:req, attack:req, decay:req, sustain:req, release:req
    // Note: Square2 has no sweep parameter unlike Square1
    else if (type.startsWith("voice_square_2") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 0x0A;
        else inst.type_id = 0x02;

        inst.base_key     = args.at(0).toInt();
        inst.pan          = args.at(1).toInt();
        inst.duty_cycle   = args.at(2).toInt();
        inst.attack       = args.at(3).toInt();
        inst.decay        = args.at(4).toInt();
        inst.sustain      = args.at(5).toInt();
        inst.release      = args.at(6).toInt();
    }

    // voice_programmable_wave base_midi_key:req, pan:req, wave_samples_pointer:req, attack:req, decay:req, sustain:req, release:req
    else if (type.startsWith("voice_programmable_wave") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 0x0B;
        else inst.type_id = 0x03;

        inst.base_key     = args.at(0).toInt();
        inst.pan          = args.at(1).toInt();
        inst.sample_label = args.at(2); // Label for the 16-byte wave data (32 4-bit samples)
        inst.attack       = args.at(3).toInt();
        inst.decay        = args.at(4).toInt();
        inst.sustain      = args.at(5).toInt();
        inst.release      = args.at(6).toInt();
    }

    // voice_noise base_midi_key:req, pan:req, period:req, attack:req, decay:req, sustain:req, release:req
    else if (type.startsWith("voice_noise") && args.size() >= 7) {
        if (type.endsWith("alt")) inst.type_id = 12;
        else inst.type_id = 4;

        inst.base_key = args.at(0).toInt();
        inst.pan = args.at(1).toInt();
        inst.duty_cycle = args.at(2).toInt(); // the "period" is stored in duty_cycle
        inst.attack = args.at(3).toInt();
        inst.decay = args.at(4).toInt();
        inst.sustain = args.at(5).toInt();
        inst.release = args.at(6).toInt();
    }

    // voice_keysplit voice_group_pointer:req, keysplit_table_pointer:req
    else if (type == "voice_keysplit" && args.size() >= 2) {
        QString name = args.at(0); // eg. "voicegroup_piano_keysplit"
        // Only remove the "voicegroup_" prefix, keep the rest of the name
        if (name.startsWith("voicegroup_")) {
            name = name.mid(11); // strlen("voicegroup_") == 11
        }

        inst.type_id = 0x40;
        inst.sample_label = name;
        inst.keysplit_table = args.at(1); // eg. "keysplit_piano"
    }

    // voice_keysplit_all voice_group_pointer:req
    else if (type == "voice_keysplit_all") {
        QString name = args.at(0);
        // Only remove the "voicegroup_" prefix, keep the rest of the name
        if (name.startsWith("voicegroup_")) {
            name = name.mid(11);
        }

        inst.type_id = 0x80;
        inst.sample_label = name;
        inst.base_key = g_midi_middle_c;
        inst.pan = 0x00;
    }

    // pass the completed instrument to the project
    this->m_project->addInstrumentToGroup(group_label, inst);
}

bool ProjectInterface::loadKeysplitTables() {
    QString text = this->readTextFile(this->concatPaths(this->m_root, "sound/keysplit_tables.inc"));
    if (text.isEmpty()) {
        return false;
    }

    QRegularExpression re_keysplit("keysplit\\s+(?<name>[A-Za-z0-9_]+),\\s+(?<offset>\\d+)");
    QRegularExpressionMatchIterator iter = re_keysplit.globalMatch(text);
    if (!iter.hasNext()) {
        return false;
    }

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured("name");
        int offset = match.captured("offset").toInt();

        // open the file located at voicegroups/keysplits/<name>.inc
        // TODO: is this (and should it be) a mandatory naming convention
        this->parseKeysplitInstrument(name);

        KeysplitTable table;
        table.label = "keysplit_" + name;
        table.offset = offset;

        // use the text block for this specific instrument to process each split index
        int start = match.capturedEnd();
        int end = text.indexOf("keysplit", start);
        if (end == -1) {
            end = text.length();
        }

        QString single_inst_text = text.mid(start, end - start);
        QRegularExpression split("split\\s+(?<i>\\d+),\\s+(?<max>\\d+)");
        QRegularExpressionMatchIterator iter_split = split.globalMatch(single_inst_text);

        int note = offset;
        while (iter_split.hasNext()) {
            QRegularExpressionMatch sm = iter_split.next();
            int i = sm.captured("i").toInt();
            int max = sm.captured("max").toInt();

            // Fill the map with the instrument index for this range
            for (int k = note; k <= max; ++k) {
                table.note_map[k] = i;
            }
            note = max + 1;
        }

        this->m_project->addTable(table.label, table);
    }

    return true;
}

void ProjectInterface::parseKeysplitInstrument(const QString &name) {
    QString text = this->readTextFile(this->concatPaths(this->m_root, "sound/voicegroups/keysplits/" + name + ".inc"));
    if (text.isEmpty()) {
        return;
    }

    QRegularExpression re_label("voice_group\\s+(?<label>[A-Za-z0-9_]+)");
    QRegularExpressionMatch match = re_label.match(text);
    if (!match.hasMatch()) {
        return;
    }

    QString group_label = match.captured("label");

    // process like any other voice group
    QRegularExpression inst("(?<type>voice_[a-z0-9_]+)\\s+(?<args>[^v]+)");
    QRegularExpressionMatchIterator iter = inst.globalMatch(text);

    while (iter.hasNext()) {
        QRegularExpressionMatch match_line = iter.next();
        QString type = match_line.captured("type");
        if (type == "voice_group") {
            continue; // skip the first line
        }

        QString raw = match_line.captured("args").simplified();
        QStringList args = raw.split(QRegularExpression(",\\s*"));

        // processInstrumentEntry(const QString &group_label, const QString &type, const QStringList &args)
        this->processInstrumentEntry(group_label, type, args);
    }
}

bool ProjectInterface::loadSongs() {
    QString text = this->readTextFile(this->concatPaths(this->m_root, "sound/song_table.inc"));
    if (text.isEmpty()) {
        return false;
    }

    QRegularExpression regex("song\\s+(?<title>[A-Za-z0-9_]+),\\s+(?<player>[A-Za-z0-9_]+),\\s+(?<type_id>[x0-9A-Fa-f]+)");
    QRegularExpressionMatchIterator iter = regex.globalMatch(text);

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();

        SongEntry song;
        song.title = match.captured("title");
        song.player = match.captured("player");

        QString type_id = match.captured("type_id");
        song.type = type_id.toInt(nullptr, 0);

        this->m_project->addSongEntry(song);
    }

    this->parseMidiConfig();
    this->validateMidiFiles();

    return true;
}

void ProjectInterface::parseMidiConfig() {
    // Parse song metadata from midi.cfg
    // Format: <song>.mid: -E -R<reverb> -G_<voicegroup> -V<volume> [-P<priority>]

    QString text = this->readTextFile(this->concatPaths(this->m_root, "sound/songs/midi/midi.cfg"));
    if (text.isEmpty()) {
        qDebug() << "parseMidiConfig: midi.cfg not found";
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
        if (!title_match.hasMatch()) {
            continue;
        }

        QString title = title_match.captured("title");

        QString voicegroup = "dummy";
        int volume = 0x7F;
        int priority = 0;
        int reverb = 0;

        QRegularExpressionMatch group_match = re_voicegroup.match(line);
        if (group_match.hasMatch()) {
            voicegroup = group_match.captured("voicegroup");
            // Strip leading underscore if present (e.g., "_abandoned_ship" -> "abandoned_ship")
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

        this->m_project->updateSongData(title, voicegroup, volume, priority, reverb);
        parsed++;
    }

    qDebug() << "parseMidiConfig: parsed" << parsed << "song configs from midi.cfg";
}

void ProjectInterface::validateMidiFiles() {
    if (!m_project) return;
    for (auto it = m_project->m_song_entries.begin(); it != m_project->m_song_entries.end(); ++it) {
        SongEntry &entry = it.value();
        if (entry.midifile.isEmpty()) {
            continue;
        }
        const QString path = this->concatPaths(this->m_root, "sound/songs/midi/" + entry.midifile);
        if (!QFile::exists(path)) {
            entry.midifile.clear();
        }
    }
}

smf::MidiFile ProjectInterface::loadMidi(const QString &title) {
    smf::MidiFile midifile;

    QString path = this->concatPaths(this->m_root, "sound/songs/midi/" + title + ".mid");

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return midifile;
    }
    std::istringstream is(f.readAll().toStdString());

    midifile.read(is);

    return midifile;

    // add song to song table
    //m_song_table.insert("mus_title", std::make_shared<Song>(midifile));

    // qDebug() << "OUTPUT FILE:\n==============================";
    // std::cout << midifile;

    //this->m_active_song = this->m_song_table.values().first();
}
