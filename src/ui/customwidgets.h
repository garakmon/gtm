#pragma once

#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QSpinBox>



class QSvgRenderer;
class QWheelEvent;
class GTMSvgIconWidget;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// Custom ComboBox adding:
///     - monospaced font styling
///
//////////////////////////////////////////////////////////////////////////////////////////
class GTMComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit GTMComboBox(QWidget *parent = nullptr);
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// Custom SpinBox adding:
///     - monospaced font styling
///
//////////////////////////////////////////////////////////////////////////////////////////
class GTMSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit GTMSpinBox(QWidget *parent = nullptr);

    void setLineEditEnabled(bool enabled);

protected:
    void wheelEvent(QWheelEvent *event) override;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// Custom LineEdit adding:
///     - support for an inline leading SVG icon inside the line edit
///
//////////////////////////////////////////////////////////////////////////////////////////
class GTMLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit GTMLineEdit(QWidget *parent = nullptr);

    void setLeadingSvg(const QString &path, int size = 14);

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLeadingMargins();
    void updateLeadingGeometry();

    QString m_leading_svg_path;
    int m_leading_svg_size = 14;
    QMargins m_base_margins;
    bool m_base_margins_set = false;
    class GTMSvgIconWidget *m_leading_icon = nullptr;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// Custom Widget (Icon) adding:
///     - recolorable svg graphics
///
//////////////////////////////////////////////////////////////////////////////////////////
class GTMSvgIconWidget : public QWidget {
    Q_OBJECT

public:
    explicit GTMSvgIconWidget(QWidget *parent = nullptr);

    void setSvgPath(const QString &path);
    void setUseTextColor(bool enabled);
    void setTintColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QSvgRenderer *m_renderer = nullptr;
    QString m_svg_path;
    QColor m_tint_color;
    bool m_use_text_color = true;
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// Custom Label adding:
///     - automatic min-width sizing based on target character count (for form alignment)
///
//////////////////////////////////////////////////////////////////////////////////////////
class GTMFormLabel : public QLabel {
    Q_OBJECT
    Q_PROPERTY(int target_chars READ targetChars WRITE setTargetChars)
    // ^ allows individual labels to set their own targets (ie in Qt creator)

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
