#ifndef VOICEGROUPEDITOR_H
#define VOICEGROUPEDITOR_H

#include "sound/soundtypes.h"

#include <QDialog>
#include <QMap>
#include <QModelIndex>
#include <QPointer>
#include <QSet>



namespace Ui {
class VoicegroupEditor;
}

class QSortFilterProxyModel;
class QStandardItemModel;
class QStackedWidget;
class QTableView;
class QCheckBox;
class VgIntSpinDelegate;
class VgMidiKeyDelegate;
class VgSampleDelegate;
class VgTypeDelegate;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// The editor window for viewing, creating, and modifying voicegroups.
///
//////////////////////////////////////////////////////////////////////////////////////////
class VoicegroupEditor : public QDialog
{
    Q_OBJECT

public:
    explicit VoicegroupEditor(QWidget *parent = nullptr);
    ~VoicegroupEditor();

    void setVoicegroups(const QMap<QString, VoiceGroup> &voicegroups);
    void setKeysplitTables(const QMap<QString, KeysplitTable> &keysplit_tables);

private:
    void setupModels();
    void setupConnections();
    void selectDefaultVoicegroup();
    void rebuildVoicegroupListModel();
    void refreshSelectedEntity();

    void refreshEntryTableForSelectedVoicegroup();
    void refreshKeysplitTableForSelectedKeysplit();
    void applyEntryFilters();
    void updateStatusText(const QString &text);
    void updateStatusForCurrentVoicegroup();
    void updateStatusForCurrentKeysplit();
    void markVoicegroupDirty(const QString &voicegroup);
    void markKeysplitDirty(const QString &keysplit);
    bool renameVoicegroupInEditModel(const QString &old_name, const QString &new_name);
    VoiceGroup *currentEditableVoicegroup();
    const VoiceGroup *currentEditableVoicegroup() const;
    KeysplitTable *currentEditableKeysplit();
    const KeysplitTable *currentEditableKeysplit() const;

    int selectedVoicegroupSourceRow() const;

    void onVoicegroupFilterTextChanged(const QString &text);
    void onVoicegroupSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onEntryModelDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right,
                                 const QList<int> &roles);
    void onKeysplitModelDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right,
                                    const QList<int> &roles);
    void onEntryFilterToggled(bool);

    void onNewVoicegroupClicked();
    void onDuplicateVoicegroupClicked();
    void onRenameVoicegroupClicked();
    void onDeleteVoicegroupClicked();
    void onSaveVoicegroupClicked();
    void onValidateVoicegroupClicked();
    void onFillRangeClicked();
    void onReplaceTypeClicked();

private:
    Ui::VoicegroupEditor *ui;

    // child widgets
    QPointer<QStandardItemModel> m_voicegroup_model;
    QPointer<QSortFilterProxyModel> m_voicegroup_filter_model;
    QPointer<QStandardItemModel> m_entry_model;
    QPointer<QStandardItemModel> m_keysplit_model;
    QPointer<QStackedWidget> m_editor_stack;
    QPointer<QTableView> m_keysplit_table_view;
    QPointer<VgTypeDelegate> m_type_delegate;
    QPointer<VgSampleDelegate> m_sample_delegate;
    QPointer<VgMidiKeyDelegate> m_key_delegate;
    QPointer<VgIntSpinDelegate> m_pan_delegate;
    QPointer<VgIntSpinDelegate> m_adsr_delegate;
    QPointer<QCheckBox> m_check_show_used_indices;

    // important source data
    QMap<QString, VoiceGroup> m_source_voicegroups;
    QMap<QString, VoiceGroup> m_edit_voicegroups;
    QMap<QString, KeysplitTable> m_source_keysplit_tables;
    QMap<QString, KeysplitTable> m_edit_keysplit_tables;
    QSet<QString> m_dirty_voicegroups;
    QSet<QString> m_dirty_keysplits;
    QString m_current_voicegroup_name;
    QString m_current_keysplit_name;
};

#endif // VOICEGROUPEDITOR_H
