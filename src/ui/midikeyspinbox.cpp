#include "midikeyspinbox.h"

#include <QWheelEvent>
#include <QLineEdit>

#include "util.h"



MidiKeySpinBox::MidiKeySpinBox(QWidget *parent)
    : QSpinBox(parent) {
    // Don't let scrolling hijack focus.
    setFocusPolicy(Qt::StrongFocus);
    this->setRange(0, 0x7F);
}

void MidiKeySpinBox::wheelEvent(QWheelEvent *event) {
    // Only allow scrolling to modify contents when it explicitly has focus.
    if (this->hasFocus()) {
        QSpinBox::wheelEvent(event);
    } else {
        event->ignore();
    }
}

void MidiKeySpinBox::setLineEditEnabled(bool enabled) {
    this->lineEdit()->setReadOnly(!enabled);
}

QString MidiKeySpinBox::textFromValue(int value) const {
    //
    return QString("%1 [%2%3]").arg(value).arg(noteValueToString(value)).arg(value / 12 - 1);
}
