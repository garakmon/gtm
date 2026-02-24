#pragma once
#ifndef MIDIKEYSPINBOX_H
#define MIDIKEYSPINBOX_H

#include "ui/customwidgets.h"



//////////////////////////////////////////////////////////////////////////////////////////
///
/// A key + note spinbox specialized for MIDI keys displaying the actual musical note.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MidiKeySpinBox : public GTMSpinBox {
    Q_OBJECT

public:
    MidiKeySpinBox(QWidget *parent = nullptr);
    void setKeySignature(int sharps_flats, bool is_minor);

protected:
    QString textFromValue(int value) const override;

private:
    int m_key_signature_sf = 0;
    bool m_key_signature_minor = false;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// A tick spinbox specialized for MIDI tick positions, with additional measure.bar text.
///
/// !TODO: measure.bar not actually implemented yet
///
//////////////////////////////////////////////////////////////////////////////////////////
class MidiTickSpinBox : public GTMSpinBox {
    Q_OBJECT

public:
    MidiTickSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(int value) const override;
};

#endif // MIDIKEYSPINBOX_H
