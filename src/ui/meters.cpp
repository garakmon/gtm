#include "ui/meters.h"

#include <QPainter>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtMath>
#include <QtGlobal>


SegmentedMeterBar::SegmentedMeterBar(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(12);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SegmentedMeterBar::setLevel(float level) {
    float clamped = qBound(0.0f, level, 1.0f);
    if (qFuzzyCompare(m_level, clamped)) return;
    m_level = clamped;
    update();
}

void SegmentedMeterBar::setSegments(int segments) {
    m_segments = qMax(1, segments);
    update();
}

void SegmentedMeterBar::setZoneColors(const QColor &low, const QColor &mid, const QColor &high) {
    m_color_low = low;
    m_color_mid = mid;
    m_color_high = high;
    update();
}

void SegmentedMeterBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int segments = qMax(1, m_segments);
    float gap = 1.0f;
    float total_gap = (segments - 1) * gap;
    float seg_w = (width() - total_gap) / segments;
    if (seg_w < 1.0f) seg_w = 1.0f;

    int lit = qBound(0, static_cast<int>(qRound(m_level * segments)), segments);
    int yellow_start = static_cast<int>(segments * 0.7f);
    int red_start = static_cast<int>(segments * 0.9f);

    float x = 0.0f;
    for (int i = 0; i < segments; ++i) {
        QColor on_color = m_color_low;
        if (i >= red_start) on_color = m_color_high;
        else if (i >= yellow_start) on_color = m_color_mid;

        QColor color = (i < lit) ? on_color : m_color_off;
        painter.fillRect(QRectF(x, 0.0f, seg_w, height()), color);
        x += seg_w + gap;
    }
}


CenteredStereoMeter::CenteredStereoMeter(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(12);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CenteredStereoMeter::setLevels(float left, float right) {
    float cl = qBound(0.0f, left, 1.0f);
    float cr = qBound(0.0f, right, 1.0f);
    if (qFuzzyCompare(m_level_left, cl) && qFuzzyCompare(m_level_right, cr)) return;
    m_level_left = cl;
    m_level_right = cr;
    update();
}

void CenteredStereoMeter::setSegmentsPerSide(int segments) {
    m_segments_per_side = qMax(1, segments);
    update();
}

void CenteredStereoMeter::setBaseColor(const QColor &color) {
    m_color_on = color;
    m_color_off = color.darker(260);
    update();
}

void CenteredStereoMeter::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int segments = qMax(1, m_segments_per_side);
    float gap = 1.0f;
    float center_gap = 2.0f;
    float center_x = width() * 0.5f;

    float side_width = (width() - center_gap * 2.0f - 1.0f) * 0.5f;
    float total_gap = (segments - 1) * gap;
    float seg_w = (side_width - total_gap) / segments;
    if (seg_w < 1.0f) seg_w = 1.0f;

    int lit_left = qBound(0, static_cast<int>(qRound(m_level_left * segments)), segments);
    int lit_right = qBound(0, static_cast<int>(qRound(m_level_right * segments)), segments);

    // Center line
    painter.fillRect(QRectF(center_x, 0.0f, 1.0f, height()), m_color_off);

    // Left segments (from center outward)
    float x_left = center_x - center_gap - seg_w;
    for (int i = 0; i < segments; ++i) {
        QColor color = (i < lit_left) ? m_color_on : m_color_off;
        painter.fillRect(QRectF(x_left, 0.0f, seg_w, height()), color);
        x_left -= seg_w + gap;
    }

    // Right segments (from center outward)
    float x_right = center_x + center_gap;
    for (int i = 0; i < segments; ++i) {
        QColor color = (i < lit_right) ? m_color_on : m_color_off;
        painter.fillRect(QRectF(x_right, 0.0f, seg_w, height()), color);
        x_right += seg_w + gap;
    }
}


MasterMeterWidget::MasterMeterWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_left = new SegmentedMeterBar(this);
    m_right = new SegmentedMeterBar(this);

    m_left->setFixedSize(150, 6);
    m_right->setFixedSize(150, 6);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName("masterVolumeSlider");
    m_slider->setRange(0, 100);
    m_slider->setValue(25);
    m_slider->setFixedWidth(150);
    m_slider->setFixedHeight(4);
    m_slider->setTickPosition(QSlider::NoTicks);

    layout->addWidget(m_left);
    layout->addWidget(m_slider);
    layout->addWidget(m_right);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(150, 24);
}

void MasterMeterWidget::setLevels(float left, float right) {
    if (m_left) m_left->setLevel(left);
    if (m_right) m_right->setLevel(right);
}
