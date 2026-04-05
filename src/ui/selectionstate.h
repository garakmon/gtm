#pragma once
#ifndef SELECTIONSTATE_H
#define SELECTIONSTATE_H

#include <QPointF>
#include <Qt>



enum class SelectionBehavior {
    Replace,
    Modify,
};

struct SelectionGestureState {
    bool pressed = false;
    bool dragging = false;
    SelectionBehavior behavior = SelectionBehavior::Replace;
};

struct PointSelectionGestureState : SelectionGestureState {
    QPointF start_scene_pos;
};

inline SelectionBehavior selectionBehaviorFromModifiers(Qt::KeyboardModifiers modifiers) {
    if (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier)) {
        return SelectionBehavior::Modify;
    }

    return SelectionBehavior::Replace;
}

#endif // SELECTIONSTATE_H
