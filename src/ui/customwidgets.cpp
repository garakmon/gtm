#include "ui/customwidgets.h"

#include <QFontMetrics>
#include <QEvent>
#include <QSvgRenderer>
#include <QPainter>
#include <QStyle>
#include <QImage>
#include <QResizeEvent>

GTMComboBox::GTMComboBox(QWidget *parent) : QComboBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
}

GTMLineEdit::GTMLineEdit(QWidget *parent) : QLineEdit(parent) {}

void GTMLineEdit::setLeadingSvg(const QString &path, int size) {
    m_leading_svg_path = path;
    m_leading_svg_size = size > 0 ? size : 14;
    if (!m_leading_icon) {
        m_leading_icon = new GTMSvgIconWidget(this);
        m_leading_icon->setUseTextColor(true);
    }
    m_leading_icon->setSvgPath(m_leading_svg_path);
    m_leading_icon->setFixedSize(m_leading_svg_size, m_leading_svg_size);
    updateLeadingMargins();
    updateLeadingGeometry();
    update();
}

void GTMLineEdit::changeEvent(QEvent *event) {
    QLineEdit::changeEvent(event);
    if (!event) {
        return;
    }
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange
        || event->type() == QEvent::PaletteChange) {
        updateLeadingMargins();
        updateLeadingGeometry();
        update();
    }
}

void GTMLineEdit::resizeEvent(QResizeEvent *event) {
    QLineEdit::resizeEvent(event);
    updateLeadingGeometry();
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

void GTMLineEdit::updateLeadingGeometry() {
    if (!m_leading_icon) {
        return;
    }
    const int icon_size = m_leading_svg_size > 0 ? m_leading_svg_size : 14;
    const int frame = style() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
    const int x = frame + 2;
    const int y = (height() - icon_size) / 2;
    m_leading_icon->setGeometry(x, y, icon_size, icon_size);
}

GTMSpinBox::GTMSpinBox(QWidget *parent) : QSpinBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
}

GTMSvgIconWidget::GTMSvgIconWidget(QWidget *parent) : QWidget(parent) {
    m_renderer = new QSvgRenderer(this);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void GTMSvgIconWidget::setSvgPath(const QString &path) {
    m_svg_path = path;
    if (m_renderer) {
        m_renderer->load(m_svg_path);
    }
    update();
}

void GTMSvgIconWidget::setUseTextColor(bool enabled) {
    if (m_use_text_color == enabled) {
        return;
    }
    m_use_text_color = enabled;
    update();
}

void GTMSvgIconWidget::setTintColor(const QColor &color) {
    m_tint_color = color;
    update();
}

void GTMSvgIconWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (!m_renderer || !m_renderer->isValid()) {
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const QSize size_px = QSize(width(), height()) * dpr;
    QImage image(size_px, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    {
        QPainter svg_painter(&image);
        svg_painter.setRenderHint(QPainter::Antialiasing, true);
        m_renderer->render(&svg_painter, QRectF(0, 0, image.width(), image.height()));
    }

    QColor tint = m_use_text_color ? palette().color(QPalette::Text) : m_tint_color;
    if (tint.isValid()) {
        QPainter tint_painter(&image);
        tint_painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint_painter.fillRect(image.rect(), tint);
    }

    image.setDevicePixelRatio(dpr);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawImage(QPointF(0, 0), image);
}

void GTMSvgIconWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event && event->type() == QEvent::PaletteChange) {
        update();
    }
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
