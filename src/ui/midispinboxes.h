#pragma once
#ifndef MIDIKEYSPINBOX_H
#define MIDIKEYSPINBOX_H

#include <QSpinBox>



class MidiSpinBox : public QSpinBox {
    Q_OBJECT

public:
    MidiSpinBox(QWidget *parent = nullptr);

    void wheelEvent(QWheelEvent *event) override;
    void setLineEditEnabled(bool enabled);
};



class MidiKeySpinBox : public MidiSpinBox {
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



class MidiTickSpinBox : public MidiSpinBox {
    Q_OBJECT

public:
    MidiTickSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(int value) const override;
};

#endif // MIDIKEYSPINBOX_H
