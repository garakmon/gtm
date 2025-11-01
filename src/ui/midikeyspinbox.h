#ifndef MIDIKEYSPINBOX_H
#define MIDIKEYSPINBOX_H

#include <QSpinBox>

class MidiKeySpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit MidiKeySpinBox(QWidget *parent = nullptr);

    void wheelEvent(QWheelEvent *event) override;
    void setLineEditEnabled(bool enabled);

protected:
    QString textFromValue(int value) const override;

private:
};

#endif // MIDIKEYSPINBOX_H
