#pragma once

#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

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

#endif // CUSTOMWIDGETS_H
