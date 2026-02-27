#include "util/util.h"

#include "util/constants.h"
#include "sound/soundtypes.h"

#include <QMap>



bool isNoteWhite(int note) {
    return !((1 << (note % 12)) & g_white_key_mask);
}

int countSetBits(int n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
}

int whiteNoteToY(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano;

    int octave = note / g_num_notes_per_octave;
    int index_in_octave = note % g_num_notes_per_octave;

    int white_index = countSetBits(g_black_key_mask & ((1 << index_in_octave) - 1));

    int pos = max_y - (octave * ui_piano_keys_octave_height
                       + ui_piano_key_white_height_sums[white_index]
                       + ui_piano_key_white_height[white_index]);

    return pos;
}

int blackNoteToY(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano;
    return max_y - ui_score_line_height * (note + 1);
}

NotePos scoreNotePosition(int note) {
    int max_y = ui_score_line_height * g_num_notes_piano - ui_piano_key_black_height;
    int y = max_y - (note * ui_score_line_height);
    return { .y = y, .height = ui_piano_key_black_height };
}

bool isNoteC(int note) {
    return !(note % 12);
}

QString instrumentTypeAbbrev(const Instrument *inst, InstrumentTypeAbbrevMode mode) {
    if (!inst) return "-";

    static const QMap<int, QString> s_base_type = {
        {0x00, "PCM"}, {0x08, "PCM"}, {0x10, "PCM"},
        {0x03, "Wave"}, {0x0B, "Wave"},
    };
    static const QMap<int, QString> s_duty_map = {
        {0, "12"}, {1, "25"}, {2, "50"}, {3, "75"}
    };

    const int type_id = inst->type_id;
    if (s_base_type.contains(type_id)) {
        return s_base_type.value(type_id);
    }

    const QString duty = s_duty_map.value(inst->duty_cycle & 0x03, "50");

    switch (type_id) {
    case 0x01:
    case 0x09:
        return QString("Sq.%1S").arg(duty);
    case 0x02:
    case 0x0A:
        return QString("Sq.%1").arg(duty);
    case 0x04:
    case 0x0C:
        return (inst->duty_cycle & 0x1) ? "Ns.7" : "Ns.15";
    case 0x40:
        if (mode == InstrumentTypeAbbrevMode::Display) return "Split";
        break;
    case 0x80:
        if (mode == InstrumentTypeAbbrevMode::Display) return "Drum";
        break;
    default:
        break;
    }

    if (mode == InstrumentTypeAbbrevMode::Playback) return "Multi";
    return "-";
}

const Instrument *resolveInstrumentForKey(
    const Instrument *inst,
    uint8_t key,
    const QMap<QString, VoiceGroup> *all_voicegroups,
    const QMap<QString, KeysplitTable> *keysplit_tables,
    const VoiceGroup **out_vg
) {
    if (out_vg) *out_vg = nullptr;
    if (!inst || !all_voicegroups) return inst;

    if (inst->type_id == 0x40) {
        auto vg_it = all_voicegroups->find(inst->sample_label);
        if (vg_it == all_voicegroups->end()) return nullptr;

        int inst_index = 0;
        if (keysplit_tables && !inst->keysplit_table.isEmpty()) {
            auto table_it = keysplit_tables->find(inst->keysplit_table);
            if (table_it != keysplit_tables->end()) {
                const KeysplitTable &table = table_it.value();
                auto note_it = table.note_map.find(key);
                if (note_it != table.note_map.end()) {
                    inst_index = note_it.value();
                } else {
                    for (auto it = table.note_map.begin(); it != table.note_map.end(); it++) {
                        if (it.key() <= key) {
                            inst_index = it.value();
                        }
                    }
                }
            }
        }

        const VoiceGroup &split_vg = vg_it.value();
        if (inst_index < 0 || inst_index >= split_vg.instruments.size()) return nullptr;
        if (out_vg) *out_vg = &split_vg;
        return resolveInstrumentForKey(&split_vg.instruments[inst_index], key,
                                       all_voicegroups, keysplit_tables, out_vg);
    }

    if (inst->type_id == 0x80) {
        auto vg_it = all_voicegroups->find(inst->sample_label);
        if (vg_it == all_voicegroups->end()) return nullptr;

        const VoiceGroup &drum_vg = vg_it.value();
        const int inst_index = key - drum_vg.offset;
        if (inst_index < 0 || inst_index >= drum_vg.instruments.size()) return nullptr;
        if (out_vg) *out_vg = &drum_vg;
        return resolveInstrumentForKey(&drum_vg.instruments[inst_index], key,
                                       all_voicegroups, keysplit_tables, out_vg);
    }

    return inst;
}

static QString s_note_strings_sharp[g_num_notes_per_octave] = {
    "C", QString::fromUtf8("C\u266F"), "D", QString::fromUtf8("D\u266F"), "E",
    "F", QString::fromUtf8("F\u266F"), "G", QString::fromUtf8("G\u266F"), "A",
    QString::fromUtf8("A\u266F"), "B"
};

static QString s_note_strings_flat[g_num_notes_per_octave] = {
    "C", QString::fromUtf8("D\u266D"), "D", QString::fromUtf8("E\u266D"), "E",
    "F", QString::fromUtf8("G\u266D"), "G", QString::fromUtf8("A\u266D"), "A",
    QString::fromUtf8("B\u266D"), "B"
};

static int pitchClassFromValue(int value) {
    int pc = value % g_num_notes_per_octave;
    if (pc < 0) pc += g_num_notes_per_octave;
    return pc;
}

QString noteValueToString(int value, int sharps_flats, bool is_minor) {
    Q_UNUSED(is_minor);
    const int pitch_class = pitchClassFromValue(value);
    if (sharps_flats < 0) {
        return s_note_strings_flat[pitch_class];
    }
    return s_note_strings_sharp[pitch_class];
}

QString noteValueToString(int value) {
    return noteValueToString(value, 0, false);
}
