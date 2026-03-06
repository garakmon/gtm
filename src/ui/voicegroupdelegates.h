#pragma once
#ifndef VOICEGROUPDELEGATES_H
#define VOICEGROUPDELEGATES_H

#include <QStyledItemDelegate>
#include <QStringList>



enum EntryColumn {
    VG_TABLE_COL_INDEX = 0,
    VG_TABLE_COL_TYPE = 1,
    VG_TABLE_COL_SAMPLE = 2,
    VG_TABLE_COL_KEY = 3,
    VG_TABLE_COL_PAN = 4,
    VG_TABLE_COL_ATTACK = 5,
    VG_TABLE_COL_DECAY = 6,
    VG_TABLE_COL_SUSTAIN = 7,
    VG_TABLE_COL_RELEASE = 8,
    VG_TABLE_COL_FLAGS = 9,
    VG_TABLE_COL_COUNT = 10,
};

class VgTypeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit VgTypeDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};

class VgSampleDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit VgSampleDelegate(QObject *parent = nullptr);
    void setDirectSoundLabels(const QStringList &labels);
    void setVoicegroupLabels(const QStringList &labels);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

private:
    QStringList m_direct_sound_labels;
    QStringList m_voicegroup_labels;
};

class VgMidiKeyDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit VgMidiKeyDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};

class VgIntSpinDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit VgIntSpinDelegate(int minimum, int maximum, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

private:
    int m_minimum = 0;
    int m_maximum = 0;
};

#endif // VOICEGROUPDELEGATES_H
