#pragma once
#ifndef GRAPHICSPIANOKEYITEM_H
#define GRAPHICSPIANOKEYITEM_H

#include <QGraphicsItem>

#include "constants.h"



class GraphicsPianoKeyItem : public QGraphicsItem {
public:
    GraphicsPianoKeyItem(int note);

    QRectF boundingRect() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual QSize dimensions() const = 0;
    void setColor(QColor color) { this->m_color = color; }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    int count_set_bits(int n) const {
        int count = 0;
        while (n > 0) {
            n &= (n - 1);
            count++;
        }
        return count;
    }

protected:
    int m_note = g_midi_middle_c;
    QColor m_color;
};



class GraphicsPianoKeyItemBlack : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemBlack(int note);
protected:
    QSize dimensions() const override { return QSize(ui_piano_key_black_width, ui_piano_key_black_height); };
};



class GraphicsPianoKeyItemWhite : public GraphicsPianoKeyItem {
public:
    GraphicsPianoKeyItemWhite(int note) : GraphicsPianoKeyItem(note) {
        this->setColor(Qt::white); // TODO: make more advanced looking with paint() reimplimentation
    }

protected:
    QSize dimensions() const override {
        int index_in_octave = m_note % g_num_notes_per_octave;
        int white_index = count_set_bits(g_black_key_mask & ((1 << index_in_octave) - 1));
        return QSize(ui_piano_key_white_width, ui_piano_key_white_height[white_index]);
    };
};

#endif // GRAPHICSPIANOKEYITEM_H
