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

protected:
    QString textFromValue(int value) const override;
};



class MidiTickSpinBox : public MidiSpinBox {
    Q_OBJECT

public:
    MidiTickSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(int value) const override;
};

#endif // MIDIKEYSPINBOX_H
