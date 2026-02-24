#include "ui/midispinboxes.h"

#include "deps/midifile/MidiEvent.h"
#include "util/constants.h"
#include "util/util.h"

#include <limits>

MidiKeySpinBox::MidiKeySpinBox(QWidget *parent) : GTMSpinBox(parent) {
    this->setRange(0, g_num_notes_piano - 1);
}

void MidiKeySpinBox::setKeySignature(int sharps_flats, bool is_minor) {
    m_key_signature_sf = sharps_flats;
    m_key_signature_minor = is_minor;
}

QString MidiKeySpinBox::textFromValue(int value) const {
    return QString("%1 [%2%3]")
        .arg(value)
        .arg(noteValueToString(value, m_key_signature_sf, m_key_signature_minor))
        .arg(value / 12 - 1);
}

MidiTickSpinBox::MidiTickSpinBox(QWidget *parent) : GTMSpinBox(parent) {
    this->setRange(0, std::numeric_limits<decltype(smf::MidiEvent::tick)>::max());
}

QString MidiTickSpinBox::textFromValue(int value) const {
    int measure = value;
    int bar = value;
    return QString("%1 [%2.%3]").arg(value).arg(measure).arg(bar);
}
