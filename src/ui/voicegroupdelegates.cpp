#include "ui/voicegroupdelegates.h"

#include "ui/customwidgets.h"
#include "ui/midispinboxes.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>



VgTypeDelegate::VgTypeDelegate(QObject *parent) : QStyledItemDelegate(parent) { }

QWidget *VgTypeDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                      const QModelIndex &) const {
    auto *combo = new GTMComboBox(parent);
    combo->addItems({"DirectSound", "Square 1", "Square 2", "Wave", "Noise", "Keysplit"});
    combo->setEditable(true);
    // https://forum.qt.io/topic/145228/qcombobox-popup-disappears
    combo->setInsertPolicy(QComboBox::NoInsert);
    if (combo->lineEdit()) {
        combo->lineEdit()->setReadOnly(true);
    }
    return combo;
}

void VgTypeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo) {
        return;
    }

    const QString value = index.data(Qt::EditRole).toString();
    const int combo_index = combo->findText(value);
    combo->setCurrentIndex(combo_index >= 0 ? combo_index : 0);
}

void VgTypeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                  const QModelIndex &index) const {
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo || !model) {
        return;
    }
    model->setData(index, combo->currentText(), Qt::EditRole);
}

VgSampleDelegate::VgSampleDelegate(QObject *parent) : QStyledItemDelegate(parent) { }

void VgSampleDelegate::setDirectSoundLabels(const QStringList &labels) {
    m_direct_sound_labels = labels;
}

void VgSampleDelegate::setVoicegroupLabels(const QStringList &labels) {
    m_voicegroup_labels = labels;
}

QWidget *VgSampleDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                        const QModelIndex &index) const {
    const QString type = index.sibling(index.row(), VG_TABLE_COL_TYPE).data().toString();
    auto *combo = new GTMComboBox(parent);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);

    if (type == "DirectSound") {
        combo->addItems(m_direct_sound_labels);
    }
    else if (type == "Keysplit") {
        combo->addItems(m_voicegroup_labels);
    }
    return combo;
}

void VgSampleDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo) {
        return;
    }

    const QString value = index.data(Qt::EditRole).toString();
    const int combo_index = combo->findText(value);
    if (combo_index >= 0) {
        combo->setCurrentIndex(combo_index);
    }
    else {
        combo->setEditText(value);
    }
}

void VgSampleDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const {
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo || !model) {
        return;
    }
    model->setData(index, combo->currentText(), Qt::EditRole);
}

VgMidiKeyDelegate::VgMidiKeyDelegate(QObject *parent) : QStyledItemDelegate(parent) { }

QWidget *VgMidiKeyDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                         const QModelIndex &) const {
    auto *spin = new MidiKeySpinBox(parent);
    spin->setRange(0, 127);
    return spin;
}

void VgMidiKeyDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    auto *spin = qobject_cast<QSpinBox *>(editor);
    if (!spin) {
        return;
    }
    spin->setValue(index.data(Qt::EditRole).toInt());
}

void VgMidiKeyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const {
    auto *spin = qobject_cast<QSpinBox *>(editor);
    if (!spin || !model) {
        return;
    }
    model->setData(index, spin->value(), Qt::EditRole);
}

VgIntSpinDelegate::VgIntSpinDelegate(int minimum, int maximum, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_minimum(minimum)
    , m_maximum(maximum) { }

QWidget *VgIntSpinDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                         const QModelIndex &) const {
    auto *spin = new GTMSpinBox(parent);
    spin->setRange(m_minimum, m_maximum);
    return spin;
}

void VgIntSpinDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    auto *spin = qobject_cast<QSpinBox *>(editor);
    if (!spin) {
        return;
    }
    spin->setValue(index.data(Qt::EditRole).toInt());
}

void VgIntSpinDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const {
    auto *spin = qobject_cast<QSpinBox *>(editor);
    if (!spin || !model) {
        return;
    }
    model->setData(index, spin->value(), Qt::EditRole);
}
