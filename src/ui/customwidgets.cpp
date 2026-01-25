#include "customwidgets.h"

#include <QFontMetrics>
#include <QEvent>
#include <QSvgRenderer>
#include <QPainter>
#include <QStyle>
#include <QImage>

GTMComboBox::GTMComboBox(QWidget *parent) : QComboBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
}

GTMLineEdit::GTMLineEdit(QWidget *parent) : QLineEdit(parent) {}

void GTMLineEdit::setLeadingSvg(const QString &path, int size) {
    m_leading_svg_path = path;
    m_leading_svg_size = size > 0 ? size : 14;
    if (!m_leading_svg) {
        m_leading_svg = new QSvgRenderer(this);
    }
    m_leading_svg->load(m_leading_svg_path);
    updateLeadingMargins();
    update();
}

void GTMLineEdit::paintEvent(QPaintEvent *event) {
    QLineEdit::paintEvent(event);
    if (!m_leading_svg || !m_leading_svg->isValid()) {
        return;
    }

    const int icon_size = m_leading_svg_size > 0 ? m_leading_svg_size : 14;
    const int frame = style() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
    const int x = frame + 2;
    const int y = (height() - icon_size) / 2;

    const qreal dpr = devicePixelRatioF();
    QImage image(icon_size * dpr, icon_size * dpr, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    {
        QPainter svg_painter(&image);
        svg_painter.setRenderHint(QPainter::Antialiasing, true);
        m_leading_svg->render(&svg_painter, QRectF(0, 0, image.width(), image.height()));
    }

    {
        QPainter tint_painter(&image);
        tint_painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint_painter.fillRect(image.rect(), palette().color(QPalette::Text));
    }

    image.setDevicePixelRatio(dpr);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawImage(QPointF(x, y), image);
}

void GTMLineEdit::changeEvent(QEvent *event) {
    QLineEdit::changeEvent(event);
    if (!event) {
        return;
    }
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        updateLeadingMargins();
        update();
    }
}

void GTMLineEdit::updateLeadingMargins() {
    if (!m_base_margins_set) {
        m_base_margins = textMargins();
        m_base_margins_set = true;
    }
    if (m_leading_svg_path.isEmpty()) {
        setTextMargins(m_base_margins);
        return;
    }
    const int icon_size = m_leading_svg_size > 0 ? m_leading_svg_size : 14;
    const int frame = style() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
    const int pad = frame + 2;
    QMargins margins = m_base_margins;
    margins.setLeft(margins.left() + icon_size + pad * 2);
    setTextMargins(margins);
}

GTMSpinBox::GTMSpinBox(QWidget *parent) : QSpinBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
}

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
