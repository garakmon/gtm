#include "ui/midispinboxes.h"

#include <QWheelEvent>
#include <QLineEdit>
#include <QFontDatabase>
#include <limits>

#include "util/util.h"
#include "util/constants.h"
#include "MidiEvent.h"



MidiSpinBox::MidiSpinBox(QWidget *parent)
    : QSpinBox(parent) {
    // Don't let scrolling hijack focus.
    setFocusPolicy(Qt::StrongFocus);
}

void MidiSpinBox::wheelEvent(QWheelEvent *event) {
    // Only allow scrolling to modify contents when it explicitly has focus.
    if (this->hasFocus()) {
        QSpinBox::wheelEvent(event);
    } else {
        event->ignore();
    }
}

void MidiSpinBox::setLineEditEnabled(bool enabled) {
    this->lineEdit()->setReadOnly(!enabled);
}



MidiKeySpinBox::MidiKeySpinBox(QWidget *parent) : MidiSpinBox(parent) {
    this->setRange(0, g_num_notes_piano - 1);
}

QString MidiKeySpinBox::textFromValue(int value) const {
    return QString("%1 [%2%3]").arg(value).arg(noteValueToString(value)).arg(value / 12 - 1);
}



MidiTickSpinBox::MidiTickSpinBox(QWidget *parent) : MidiSpinBox(parent) {
    this->setRange(0, std::numeric_limits<decltype(smf::MidiEvent::tick)>::max());
}

QString MidiTickSpinBox::textFromValue(int value) const {
    int measure = value;
    int bar = value;
    return QString("%1 [%2.%3]").arg(value).arg(measure).arg(bar);
}
