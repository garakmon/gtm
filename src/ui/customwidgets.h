#pragma once

#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

class GTMComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit GTMComboBox(QWidget *parent = nullptr);
};



class GTMLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit GTMLineEdit(QWidget *parent = nullptr);
};



class GTMSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit GTMSpinBox(QWidget *parent = nullptr);
};



class GTMFormLabel : public QLabel {
    Q_OBJECT
    Q_PROPERTY(int target_chars READ targetChars WRITE setTargetChars)
    // ^ allows individual labels to set their own targets (like in Qt creator)

public:
    explicit GTMFormLabel(QWidget *parent = nullptr);

    int targetChars() const { return m_target_chars; }
    void setTargetChars(int chars);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateMinimumWidth();

    int m_target_chars = 15;
};

#endif // CUSTOMWIDGETS_H
