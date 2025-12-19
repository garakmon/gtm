#pragma once
#ifndef MINIMAPWIDGET_H
#define MINIMAPWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPointer>
#include <memory>

class Song;
class QScrollBar;
class QGraphicsView;

class MinimapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MinimapWidget(QWidget *parent = nullptr);

    void setSong(std::shared_ptr<Song> song);
    void setScrollBar(QScrollBar *scroll);
    void setView(QGraphicsView *view);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void rebuildCache();
    void scrollToX(int x);

    std::shared_ptr<Song> m_song;
    QPointer<QScrollBar> m_scroll;
    QPointer<QGraphicsView> m_view;

    QImage m_cache;
    bool m_dirty = true;
    int m_total_ticks = 0;
};

#endif // MINIMAPWIDGET_H
