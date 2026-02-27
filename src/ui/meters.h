#pragma once
#ifndef METERS_H
#define METERS_H

#include <QColor>
#include <QPointer>
#include <QSlider>
#include <QWidget>



//////////////////////////////////////////////////////////////////////////////////////////
///
/// A SegmentedMeterBar is a 1-dimensional level meter widget for volume indication.
///
//////////////////////////////////////////////////////////////////////////////////////////
class SegmentedMeterBar : public QWidget {
    Q_OBJECT

public:
    explicit SegmentedMeterBar(QWidget *parent = nullptr);

    void setLevel(float level);
    void setSegments(int segments);
    void setZoneColors(const QColor &low, const QColor &mid, const QColor &high);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_segments = 24;
    float m_level = 0.0f;
    QColor m_color_low = QColor(0x4c, 0xaf, 0x50); // green
    QColor m_color_mid = QColor(0xff, 0xc1, 0x07); // amber
    QColor m_color_high = QColor(0xe5, 0x39, 0x35); // red
    QColor m_color_off = QColor(0x2b, 0x2b, 0x2b);
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// A CenteredStereoMeter is a 1-dimensional level meter widget for volume indication.
/// This widget uses a symmetric stereo level display.
///
//////////////////////////////////////////////////////////////////////////////////////////
class CenteredStereoMeter : public QWidget {
    Q_OBJECT

public:
    explicit CenteredStereoMeter(QWidget *parent = nullptr);

    void setLevels(float left, float right);
    void setSegmentsPerSide(int segments);
    void setBaseColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_segments_per_side = 12;
    float m_level_left = 0.0f;
    float m_level_right = 0.0f;
    QColor m_color_on = QColor(0x2e, 0xcc, 0x71);
    QColor m_color_off = QColor(0x1d, 0x1d, 0x1d);
};



//////////////////////////////////////////////////////////////////////////////////////////
///
/// The MasterMeterWidget combines two horizontal output meters with a volume slider.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MasterMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit MasterMeterWidget(QWidget *parent = nullptr);

    void setLevels(float left, float right);
    QSlider *slider() const { return m_slider; }

private:
    QPointer<SegmentedMeterBar> m_left;
    QPointer<SegmentedMeterBar> m_right;
    QPointer<QSlider> m_slider;
};

#endif // METERS_H
