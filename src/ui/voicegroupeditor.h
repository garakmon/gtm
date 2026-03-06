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
    void populateEntryInspectorTypeChoices();
    void selectDefaultVoicegroup();
    void rebuildVoicegroupListModel();

    void refreshEntryTableForSelectedVoicegroup();
    void applyEntryFilters();
    void syncInspectorFromSelectedEntry();
    void syncSelectedEntryFromInspector();
    void updateKeysplitPreview();
    void updateStatusText(const QString &text);
    void updateStatusForCurrentVoicegroup();
    void markVoicegroupDirty(const QString &voicegroup);
    bool renameVoicegroupInEditModel(const QString &old_name, const QString &new_name);
    VoiceGroup *currentEditableVoicegroup();
    const VoiceGroup *currentEditableVoicegroup() const;

    int selectedVoicegroupSourceRow() const;
    int selectedEntryRow() const;

    void onVoicegroupFilterTextChanged(const QString &text);
    void onVoicegroupSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onEntrySelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onEntryModelDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right,
                                 const QList<int> &roles);
    void onEntryFilterToggled(bool);
    void onInspectorFieldChanged();

    void onNewVoicegroupClicked();
    void onDuplicateVoicegroupClicked();
    void onRenameVoicegroupClicked();
    void onDeleteVoicegroupClicked();
    void onSaveVoicegroupClicked();
    void onValidateVoicegroupClicked();
    void onFillRangeClicked();
    void onReplaceTypeClicked();
    void onNormalizePlaceholdersClicked();

private:
    Ui::VoicegroupEditor *ui;

    // child widgets
    QPointer<QStandardItemModel> m_voicegroup_model;
    QPointer<QSortFilterProxyModel> m_voicegroup_filter_model;
    QPointer<QStandardItemModel> m_entry_model;
    QPointer<QStandardItemModel> m_keysplit_model;
    QPointer<VgTypeDelegate> m_type_delegate;
    QPointer<VgSampleDelegate> m_sample_delegate;
    QPointer<VgMidiKeyDelegate> m_key_delegate;
    QPointer<VgIntSpinDelegate> m_pan_delegate;
    QPointer<VgIntSpinDelegate> m_adsr_delegate;

    // important source data
    QMap<QString, VoiceGroup> m_source_voicegroups;
    QMap<QString, VoiceGroup> m_edit_voicegroups;
    QMap<QString, KeysplitTable> m_keysplit_tables;
    QSet<QString> m_dirty_voicegroups;
    QString m_current_voicegroup_name;
    bool m_updating_inspector = false;
};

#endif // VOICEGROUPEDITOR_H
