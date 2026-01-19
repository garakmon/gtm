#include "customwidgets.h"

#include <QFontMetrics>
#include <QEvent>

GTMComboBox::GTMComboBox(QWidget *parent) : QComboBox(parent) {}

GTMLineEdit::GTMLineEdit(QWidget *parent) : QLineEdit(parent) {}

GTMSpinBox::GTMSpinBox(QWidget *parent) : QSpinBox(parent) {}

GTMFormLabel::GTMFormLabel(QWidget *parent) : QLabel(parent) {
    updateMinimumWidth();
}

void GTMFormLabel::setTargetChars(int chars) {
    if (chars <= 0) {
        return;
    }
    if (m_target_chars == chars) {
        return;
    }
    m_target_chars = chars;
    updateMinimumWidth();
}

void GTMFormLabel::changeEvent(QEvent *event) {
    QLabel::changeEvent(event);
    if (event && event->type() == QEvent::FontChange) {
        updateMinimumWidth();
    }
}

void GTMFormLabel::updateMinimumWidth() {
    QFontMetrics fm(font());
    const int width = fm.horizontalAdvance(QString(m_target_chars, 'X'));
    setMinimumWidth(width);
}
