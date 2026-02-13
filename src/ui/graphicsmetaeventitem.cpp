#include "ui/graphicsmetaeventitem.h"

#include <cmath>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

#include "ui/colors.h"
#include "util/constants.h"
#include "deps/midifile/MidiEvent.h"

GraphicsMetaEventItem::GraphicsMetaEventItem(smf::MidiEvent *event, MetaType type, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_event(event), m_type(type) {

    setFlag(QGraphicsItem::ItemIsSelectable);

    switch (type) {
        case MetaType::TimeSignature: {
            int num = (*event)[3];
            int den = static_cast<int>(std::pow(2, (*event)[4]));
            m_label = QString("%1/%2").arg(num).arg(den);
            break;
        }
        case MetaType::Tempo: {
            double bpm = event->getTempoBPM();
            m_label = QString("%1 BPM").arg(bpm, 0, 'f', 1);
            break;
        }
        case MetaType::KeySignature: {
            m_label = "Key";
            break;
        }
        case MetaType::Marker: {
            m_label = QString::fromStdString(event->getMetaContent());
            break;
        }
    }
}

QRectF GraphicsMetaEventItem::boundingRect() const {
    int x = m_event->tick * ui_tick_x_scale;
    int y = 32;
    int width = 40;
    int height = 14;
    return QRectF(x, y, width, height);
}

void GraphicsMetaEventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    QColor bg;
    switch (this->m_type) {
        case MetaType::TimeSignature:
            bg = ui_meta_event_timesignature;
            break;
        case MetaType::Tempo:
            bg = ui_meta_event_tempo;
            break;
        case MetaType::KeySignature:
            bg = ui_meta_event_keysignature;
            break;
        case MetaType::Marker:
            bg = ui_meta_event_marker;
            break;
    }

    painter->setBrush(bg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(boundingRect(), 3, 3);

    painter->setPen(bg.lightness() > 100 ? Qt::black : Qt::white);
    QFont font = painter->font();
    font.setPixelSize(9);
    painter->setFont(font);
    painter->drawText(boundingRect().adjusted(3, 0, -3, 0), Qt::AlignVCenter, m_label);
}

void GraphicsMetaEventItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mousePressEvent(event);
}
