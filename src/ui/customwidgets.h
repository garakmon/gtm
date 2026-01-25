#pragma once

#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

class QSvgRenderer;

class GTMComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit GTMComboBox(QWidget *parent = nullptr);
};



class GTMLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit GTMLineEdit(QWidget *parent = nullptr);

    void setLeadingSvg(const QString &path, int size = 14);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateLeadingMargins();

    QString m_leading_svg_path;
    int m_leading_svg_size = 14;
    QMargins m_base_margins;
    bool m_base_margins_set = false;
    class QSvgRenderer *m_leading_svg = nullptr;
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
