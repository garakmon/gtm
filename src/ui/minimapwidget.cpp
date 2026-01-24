#include "minimapwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QGraphicsView>

#include "song.h"
#include "MidiEvent.h"
#include "colors.h"
#include "constants.h"

MinimapWidget::MinimapWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(30);
    setMaximumHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TranslucentBackground);
}

void MinimapWidget::setSong(std::shared_ptr<Song> song) {
    m_song = std::move(song);
    m_total_ticks = m_song ? m_song->durationInTicks() : 0;
    m_dirty = true;
    update();
}

void MinimapWidget::setScrollBar(QScrollBar *scroll) {
    m_scroll = scroll;
    if (m_scroll) {
        connect(m_scroll, &QScrollBar::valueChanged, this, [this]() { update(); });
        connect(m_scroll, &QScrollBar::rangeChanged, this, [this]() { update(); });
    }
}

void MinimapWidget::setView(QGraphicsView *view) {
    m_view = view;
}

void MinimapWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    if (m_dirty) {
        rebuildCache();
    }

    QPainter painter(this);
    if (!m_song || m_total_ticks <= 0) {
        // Leave transparent when no song is loaded.
        return;
    }
    painter.fillRect(rect(), ui_color_piano_roll_bg.darker(120));

    if (!m_cache.isNull()) {
        painter.drawImage(0, 0, m_cache);
    }

    if (!m_song || !m_scroll || !m_view || m_total_ticks <= 0) return;

    const int view_width_px = m_view->viewport()->width();
    const int total_width = width();
    if (total_width <= 0) return;

    const double ticks_per_px = 1.0 / static_cast<double>(ui_tick_x_scale);
    const double view_ticks = view_width_px * ticks_per_px;
    const double start_tick = m_scroll->value() * ticks_per_px;

    const double scale_x = static_cast<double>(total_width) / static_cast<double>(m_total_ticks);
    int x = static_cast<int>(start_tick * scale_x);
    int w = static_cast<int>(view_ticks * scale_x);
    if (w < 1) w = 1;
    if (x < 0) x = 0;
    if (x + w > total_width) w = total_width - x;

    QRect view_rect(x, 0, w, height() - 1);
    painter.setPen(QPen(ui_color_piano_roll_bg, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(view_rect);

    // Tint inside viewport
    QColor tint = Qt::white;
    tint.setAlpha(70);
    painter.fillRect(view_rect.adjusted(1, 1, -1, -1), tint);
}

void MinimapWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_dirty = true;
}

void MinimapWidget::mousePressEvent(QMouseEvent *event) {
    scrollToX(event->pos().x());
}

void MinimapWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        scrollToX(event->pos().x());
    }
}

void MinimapWidget::rebuildCache() {
    m_dirty = false;
    m_cache = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(Qt::transparent);

    if (!m_song || m_total_ticks <= 0) return;

    QPainter p(&m_cache);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int h = height();
    const int w = width();
    if (w <= 0 || h <= 0) return;

    const double scale_x = static_cast<double>(w) / static_cast<double>(m_total_ticks);
    const double scale_y = static_cast<double>(h - 1) / 127.0;

    // Horizontal octave lines (subtle, alternating) - draw as fills (no border)
    for (int key = 0, idx = 0; key < 128; key += 12, ++idx) {
        QColor line = (idx % 2 == 0) ? ui_color_score_line_dark : ui_color_score_line_light;
        int y = h - 1 - static_cast<int>(key * scale_y);
        if (y >= 0 && y < h) {
            p.fillRect(0, y, w, 1, line);
        }
    }

    const auto &notes = m_song->getNotes();
    for (const auto &pair : notes) {
        const int track_num = pair.first;
        if (m_song->isMetaTrack(track_num)) continue;

        smf::MidiEvent *note_on = pair.second;
        if (!note_on) continue;
        int key = note_on->getKeyNumber();
        int start_tick = note_on->tick;
        int end_tick = start_tick + 1;
        if (note_on->hasLink()) {
            end_tick = note_on->getLinkedEvent()->tick;
        }
        if (end_tick < start_tick) end_tick = start_tick;

        int x1 = static_cast<int>(start_tick * scale_x);
        int x2 = static_cast<int>(end_tick * scale_x);
        if (x2 < x1) x2 = x1;
        if (x1 >= w) continue;
        if (x2 >= w) x2 = w - 1;

        int y = h - 1 - static_cast<int>(key * scale_y);
        if (y < 0 || y >= h) continue;

        int display_row = m_song->getDisplayRow(track_num);
        int color_index = (display_row >= 0 && display_row < g_max_num_tracks) ? display_row : 0;
        p.setPen(QPen(ui_track_color_array[color_index], 1));
        p.drawLine(x1, y, x2, y);
    }
}

void MinimapWidget::scrollToX(int x) {
    if (!m_song || !m_scroll || !m_view || m_total_ticks <= 0) return;
    const int w = width();
    if (w <= 0) return;

    const double view_ticks = m_view->viewport()->width() / static_cast<double>(ui_tick_x_scale);
    const double target_tick = (static_cast<double>(x) / w) * m_total_ticks;
    double start_tick = target_tick - (view_ticks / 2.0);
    if (start_tick < 0) start_tick = 0;
    if (start_tick + view_ticks > m_total_ticks) {
        start_tick = m_total_ticks - view_ticks;
        if (start_tick < 0) start_tick = 0;
    }
    int start_px = static_cast<int>(start_tick * ui_tick_x_scale);
    m_scroll->setValue(start_px);
}
