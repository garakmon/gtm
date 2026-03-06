#include "ui/customwidgets.h"

#include <QCompleter>
#include <QEvent>
#include <QFontMetrics>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QSvgRenderer>



GTMComboBox::GTMComboBox(QWidget *parent) : QComboBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);

    // allow items to be searched by any part of the word, displaying all matches
    setEditable(true);
    this->completer()->setCompletionMode(QCompleter::PopupCompletion);
    this->completer()->setFilterMode(Qt::MatchContains);
}



//////////////////////////////////////////////////////////////////////////////////////////



GTMSpinBox::GTMSpinBox(QWidget *parent) : QSpinBox(parent) {
    QFont mono("IBM Plex Mono");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
    setFocusPolicy(Qt::StrongFocus);

    setMinimumWidth(20);
}

void GTMSpinBox::setLineEditEnabled(bool enabled) {
    this->lineEdit()->setReadOnly(!enabled);
}

void GTMSpinBox::wheelEvent(QWheelEvent *event) {
    if (this->hasFocus()) {
        QSpinBox::wheelEvent(event);
    } else {
        event->ignore();
    }
}



//////////////////////////////////////////////////////////////////////////////////////////



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


//////////////////////////////////////////////////////////////////////////////////////////



GTMPrefixedLineEdit::GTMPrefixedLineEdit(QWidget *parent) : GTMLineEdit(parent) {
    connect(this, &QLineEdit::textChanged, this, [this](const QString &) {
        this->ensureFixedPrefix();
    });
}

GTMPrefixedLineEdit::GTMPrefixedLineEdit(const QString &prefix, QWidget *parent)
  : GTMPrefixedLineEdit(parent) {
    this->setFixedPrefix(prefix);
}

void GTMPrefixedLineEdit::setFixedPrefix(const QString &prefix) {
    m_fixed_prefix = prefix;
    this->ensureFixedPrefix();
}

QString GTMPrefixedLineEdit::suffixText() const {
    if (m_fixed_prefix.isEmpty()) {
        return this->text();
    }
    if (!this->text().startsWith(m_fixed_prefix)) {
        return this->text();
    }
    return this->text().mid(m_fixed_prefix.size());
}

void GTMPrefixedLineEdit::keyPressEvent(QKeyEvent *event) {
    if (!event || m_fixed_prefix.isEmpty()) {
        GTMLineEdit::keyPressEvent(event);
        return;
    }

    const int boundary = this->prefixBoundary();
    const int cursor = this->cursorPosition();

    if (event->key() == Qt::Key_Backspace && cursor <= boundary) {
        this->setCursorPosition(boundary);
        return;
    }
    if (event->key() == Qt::Key_Left && cursor <= boundary) {
        this->setCursorPosition(boundary);
        return;
    }
    if (event->key() == Qt::Key_Home) {
        this->setCursorPosition(boundary);
        return;
    }

    GTMLineEdit::keyPressEvent(event);
    if (this->cursorPosition() < boundary) {
        this->setCursorPosition(boundary);
    }
}

void GTMPrefixedLineEdit::mousePressEvent(QMouseEvent *event) {
    GTMLineEdit::mousePressEvent(event);
    if (m_fixed_prefix.isEmpty()) {
        return;
    }

    const int boundary = this->prefixBoundary();
    if (this->cursorPosition() < boundary) {
        this->setCursorPosition(boundary);
    }
}

void GTMPrefixedLineEdit::ensureFixedPrefix() {
    if (m_is_enforcing || m_fixed_prefix.isEmpty()) {
        return;
    }

    const QString current = this->text();
    if (current.startsWith(m_fixed_prefix)) {
        return;
    }

    int overlap = 0;
    const int max = qMin(current.size(), m_fixed_prefix.size());
    for (int i = max; i > 0; --i) {
        if (current.startsWith(m_fixed_prefix.left(i))) {
            overlap = i;
            break;
        }
    }

    const QString corrected = m_fixed_prefix + current.mid(overlap);

    m_is_enforcing = true;
    QSignalBlocker blocker(this);
    this->setText(corrected);
    this->setCursorPosition(qMax(this->cursorPosition(), this->prefixBoundary()));
    m_is_enforcing = false;
}

int GTMPrefixedLineEdit::prefixBoundary() const {
    return m_fixed_prefix.size();
}



//////////////////////////////////////////////////////////////////////////////////////////



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


//////////////////////////////////////////////////////////////////////////////////////////



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



//////////////////////////////////////////////////////////////////////////////////////////



GTMFormLayout::GTMFormLayout(QWidget *parent) : QFormLayout(parent) {}

void GTMFormLayout::addRow(const QString &label_text, QWidget *field) {
    if (!field) {
        return;
    }

    auto *label = new GTMFormLabel();
    label->setText(label_text);
    QFormLayout::addRow(label, field);

    if (!m_field_widgets.contains(field)) {
        m_field_widgets.append(field);
    }
    this->syncFieldWidths();
}

void GTMFormLayout::syncFieldWidths() {
    int max_width = 0;
    for (QWidget *field : m_field_widgets) {
        if (!field) {
            continue;
        }
        max_width = qMax(max_width, field->sizeHint().width());
    }

    for (QWidget *field : m_field_widgets) {
        if (!field) {
            continue;
        }
        field->setMinimumWidth(max_width);
    }
}
