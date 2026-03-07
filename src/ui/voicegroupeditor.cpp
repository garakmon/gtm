#include "ui/voicegroupeditor.h"

#include "ui/voicegroupdelegates.h"
#include "ui_voicegroupeditor.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QFontMetrics>



static constexpr int k_role_is_used = Qt::UserRole + 1;
static constexpr int k_role_has_error = Qt::UserRole + 2;
static constexpr int k_role_entity_kind = Qt::UserRole + 10;
static constexpr int k_role_entity_name = Qt::UserRole + 11;

enum class EntityKind : int {
    Voicegroup = 0,
    Keysplit = 1,
};

static QString displayTypeName(const Instrument &inst) {
    if (inst.type.startsWith("voice_directsound")) return "DirectSound";
    if (inst.type.startsWith("voice_square_1")) return "Square 1";
    if (inst.type.startsWith("voice_square_2")) return "Square 2";
    if (inst.type.startsWith("voice_programmable_wave")) return "Wave";
    if (inst.type.startsWith("voice_noise")) return "Noise";
    if (inst.type.startsWith("voice_keysplit")) return "Keysplit";
    return inst.type;
}

static QString internalTypeNameFromDisplay(const QString &display, const QString &fallback) {
    if (display == "DirectSound") return "voice_directsound";
    if (display == "Square 1") return "voice_square_1";
    if (display == "Square 2") return "voice_square_2";
    if (display == "Wave") return "voice_programmable_wave";
    if (display == "Noise") return "voice_noise";
    if (display == "Keysplit") return "voice_keysplit";
    return fallback;
}

static QStringList directSoundLabelsFromVoicegroups(const QMap<QString, VoiceGroup> &voicegroups) {
    QSet<QString> labels;
    for (auto vg_it = voicegroups.constBegin(); vg_it != voicegroups.constEnd(); ++vg_it) {
        for (const Instrument &inst : vg_it.value().instruments) {
            if (inst.type.startsWith("voice_directsound") && !inst.sample_label.isEmpty()) {
                labels.insert(inst.sample_label);
            }
        }
    }
    QStringList list = labels.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}

static int tableColumnWidthForText(const QTableView *table, const QString &sample_text,
                                   int extra_padding = 16) {
    if (!table) {
        return 64;
    }
    const QFontMetrics fm(table->font());
    return fm.horizontalAdvance(sample_text) + extra_padding;
}

VoicegroupEditor::VoicegroupEditor(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VoicegroupEditor) {
    ui->setupUi(this);

    if (ui->horizontalLayout_EntryTableControls) {
        m_check_show_used_indices = new QCheckBox("Show used indices", this);
        ui->horizontalLayout_EntryTableControls->addWidget(m_check_show_used_indices);
    }

    if (ui->widget_RightPane) {
        ui->widget_RightPane->hide();
    }
    if (ui->splitter_Main) {
        ui->splitter_Main->setStretchFactor(0, 0);
        ui->splitter_Main->setStretchFactor(1, 1);
        ui->splitter_Main->setStretchFactor(2, 0);
        ui->splitter_Main->setSizes({220, 980, 0});
    }

    this->setupModels();
    this->setupConnections();
    this->rebuildVoicegroupListModel();

    if (ui->checkBox_ShowUsedOnly) {
        ui->checkBox_ShowUsedOnly->setText("Show unique");
    }
}

VoicegroupEditor::~VoicegroupEditor() {
    delete ui;
}

/**
 * Setup list/table models, assign them to views, and configure delegates
 * for editable entry columns.
 */
void VoicegroupEditor::setupModels() {
    m_voicegroup_model = new QStandardItemModel(this);
    m_voicegroup_filter_model = new QSortFilterProxyModel(this);
    m_entry_model = new QStandardItemModel(this);
    m_keysplit_model = new QStandardItemModel(this);

    if (m_voicegroup_filter_model && m_voicegroup_model) {
        m_voicegroup_filter_model->setSourceModel(m_voicegroup_model);
        m_voicegroup_filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_voicegroup_filter_model->setFilterKeyColumn(0);
    }

    if (ui->listView_Voicegroups && m_voicegroup_filter_model) {
        ui->listView_Voicegroups->setModel(m_voicegroup_filter_model);
        ui->listView_Voicegroups->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->listView_Voicegroups->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    if (m_entry_model) {
        m_entry_model->setColumnCount(VG_TABLE_COL_COUNT);
        m_entry_model->setHorizontalHeaderLabels({
            "Prg",
            "Type",
            "Sample/Subgroup",
            "Key",
            "Pan",
            "Atk",
            "Dec",
            "Sus",
            "Rel",
            "Metadata",
        });
    }
    if (ui->tableView_VoicegroupEntries && m_entry_model) {
        ui->tableView_VoicegroupEntries->setModel(m_entry_model);
        ui->tableView_VoicegroupEntries->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->tableView_VoicegroupEntries->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->tableView_VoicegroupEntries->setEditTriggers(
            QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
        );
        ui->tableView_VoicegroupEntries->setAlternatingRowColors(true);
        ui->tableView_VoicegroupEntries->setWordWrap(false);
        ui->tableView_VoicegroupEntries->verticalHeader()->setVisible(false);
        ui->tableView_VoicegroupEntries->horizontalHeader()->setStretchLastSection(false);
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_INDEX, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_INDEX,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "127")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_TYPE, QHeaderView::ResizeToContents
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_TYPE,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "DirectSound")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_SAMPLE, QHeaderView::Stretch
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_KEY, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_KEY,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "127 [C#-1]")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_PAN, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_PAN,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "-127")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_ATTACK, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_ATTACK,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "255")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_DECAY, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_DECAY,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "255")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_SUSTAIN, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_SUSTAIN,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "255")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_RELEASE, QHeaderView::Interactive
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_RELEASE,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "255")
        );
        ui->tableView_VoicegroupEntries->horizontalHeader()->setSectionResizeMode(
            VG_TABLE_COL_FLAGS, QHeaderView::ResizeToContents
        );
        ui->tableView_VoicegroupEntries->setColumnWidth(
            VG_TABLE_COL_FLAGS,
            tableColumnWidthForText(ui->tableView_VoicegroupEntries, "voicegroup_keysplit_label")
        );

        m_type_delegate = new VgTypeDelegate(this);
        m_sample_delegate = new VgSampleDelegate(this);
        m_key_delegate = new VgMidiKeyDelegate(this);
        m_pan_delegate = new VgIntSpinDelegate(-127, 127, this);
        m_adsr_delegate = new VgIntSpinDelegate(0, 255, this);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_TYPE, m_type_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_SAMPLE, m_sample_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_KEY, m_key_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_PAN, m_pan_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_ATTACK, m_adsr_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_DECAY, m_adsr_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_SUSTAIN, m_adsr_delegate);
        ui->tableView_VoicegroupEntries->setItemDelegateForColumn(VG_TABLE_COL_RELEASE, m_adsr_delegate);
    }

    if (m_keysplit_model) {
        m_keysplit_model->setColumnCount(2);
        m_keysplit_model->setHorizontalHeaderLabels({"Note", "Program"});
    }
    if (ui->tableView_VoicegroupEntries && m_keysplit_model) {
        m_keysplit_table_view = new QTableView(ui->tableView_VoicegroupEntries->parentWidget());
        m_keysplit_table_view->setModel(m_keysplit_model);
        m_keysplit_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_keysplit_table_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_keysplit_table_view->setEditTriggers(
            QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
        );
        m_keysplit_table_view->setAlternatingRowColors(true);
        m_keysplit_table_view->setWordWrap(false);
        m_keysplit_table_view->verticalHeader()->setVisible(false);
        m_keysplit_table_view->horizontalHeader()->setStretchLastSection(true);
        m_keysplit_table_view->setColumnWidth(
            0, tableColumnWidthForText(m_keysplit_table_view, "127")
        );
        m_keysplit_table_view->setColumnWidth(
            1, tableColumnWidthForText(m_keysplit_table_view, "127")
        );
    }

    if (ui->tableView_VoicegroupEntries && m_keysplit_table_view
        && ui->tableView_VoicegroupEntries->parentWidget()) {
        if (auto *layout = qobject_cast<QVBoxLayout *>(
                ui->tableView_VoicegroupEntries->parentWidget()->layout())) {
            layout->addWidget(m_keysplit_table_view);
        }
        m_keysplit_table_view->hide();
    }
}

/**
 * Connect ui signals for selection/filtering, inspector sync, and toolbar actions.
 */
void VoicegroupEditor::setupConnections() {
    if (ui->lineEdit_VoicegroupFilter && m_voicegroup_filter_model) {
        connect(ui->lineEdit_VoicegroupFilter, &QLineEdit::textChanged, this,
                &VoicegroupEditor::onVoicegroupFilterTextChanged);
    }

    if (ui->listView_Voicegroups && ui->listView_Voicegroups->selectionModel()) {
        connect(ui->listView_Voicegroups->selectionModel(), &QItemSelectionModel::currentChanged,
                this, &VoicegroupEditor::onVoicegroupSelectionChanged);
    }

    if (m_entry_model) {
        connect(m_entry_model, &QStandardItemModel::dataChanged, this,
                &VoicegroupEditor::onEntryModelDataChanged);
    }
    if (m_keysplit_model) {
        connect(m_keysplit_model, &QStandardItemModel::dataChanged, this,
                &VoicegroupEditor::onKeysplitModelDataChanged);
    }

    if (ui->checkBox_ShowUsedOnly) {
        connect(ui->checkBox_ShowUsedOnly, &QCheckBox::toggled, this,
                &VoicegroupEditor::onEntryFilterToggled);
    }
    if (ui->checkBox_ShowErrorsOnly) {
        connect(ui->checkBox_ShowErrorsOnly, &QCheckBox::toggled, this,
                &VoicegroupEditor::onEntryFilterToggled);
    }

    if (ui->button_NewVoicegroup) {
        connect(ui->button_NewVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onNewVoicegroupClicked);
    }

    if (ui->button_DuplicateVoicegroup) {
        connect(ui->button_DuplicateVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onDuplicateVoicegroupClicked);
    }

    if (ui->button_RenameVoicegroup) {
        connect(ui->button_RenameVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onRenameVoicegroupClicked);
    }

    if (ui->button_DeleteVoicegroup) {
        connect(ui->button_DeleteVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onDeleteVoicegroupClicked);
    }

    if (ui->button_SaveVoicegroup) {
        connect(ui->button_SaveVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onSaveVoicegroupClicked);
    }
    if (ui->button_ValidateVoicegroup) {
        connect(ui->button_ValidateVoicegroup, &QPushButton::clicked, this,
                &VoicegroupEditor::onValidateVoicegroupClicked);
    }
    if (ui->button_FillRange) {
        connect(ui->button_FillRange, &QPushButton::clicked, this,
                &VoicegroupEditor::onFillRangeClicked);
    }
    if (ui->button_ReplaceType) {
        connect(ui->button_ReplaceType, &QPushButton::clicked, this,
                &VoicegroupEditor::onReplaceTypeClicked);
    }
}

void VoicegroupEditor::onVoicegroupFilterTextChanged(const QString &text) {
    if (!m_voicegroup_filter_model) {
        return;
    }
    m_voicegroup_filter_model->setFilterFixedString(text.trimmed());
}

void VoicegroupEditor::onVoicegroupSelectionChanged(const QModelIndex &, const QModelIndex &) {
    this->refreshSelectedEntity();
}

void VoicegroupEditor::refreshSelectedEntity() {
    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0 || !m_voicegroup_model) {
        m_current_voicegroup_name.clear();
        m_current_keysplit_name.clear();
        return;
    }

    const QModelIndex idx = m_voicegroup_model->index(row, 0);
    const EntityKind kind = static_cast<EntityKind>(idx.data(k_role_entity_kind).toInt());
    if (kind == EntityKind::Keysplit) {
        this->refreshKeysplitTableForSelectedKeysplit();
    } else {
        this->refreshEntryTableForSelectedVoicegroup();
    }
}

/**
 * Mirror edits made directly in the entries table into the editable voicegroup
 * model and mark the active voicegroup as dirty.
 */
void VoicegroupEditor::onEntryModelDataChanged(const QModelIndex &top_left,
                                               const QModelIndex &bottom_right,
                                               const QList<int> &) {
    VoiceGroup *group = this->currentEditableVoicegroup();
    if (!group || !m_entry_model) {
        return;
    }

    const int start_row = qMax(0, top_left.row());
    const int end_row = qMin(bottom_right.row(), group->instruments.size() - 1);
    for (int row = start_row; row <= end_row; ++row) {
        Instrument &inst = group->instruments[row];
        inst.type = internalTypeNameFromDisplay(
            m_entry_model->index(row, VG_TABLE_COL_TYPE).data().toString(), inst.type
        );
        inst.sample_label = m_entry_model->index(row, VG_TABLE_COL_SAMPLE).data().toString();
        inst.base_key = m_entry_model->index(row, VG_TABLE_COL_KEY).data().toInt();
        inst.pan = m_entry_model->index(row, VG_TABLE_COL_PAN).data().toInt();
        inst.attack = m_entry_model->index(row, VG_TABLE_COL_ATTACK).data().toInt();
        inst.decay = m_entry_model->index(row, VG_TABLE_COL_DECAY).data().toInt();
        inst.sustain = m_entry_model->index(row, VG_TABLE_COL_SUSTAIN).data().toInt();
        inst.release = m_entry_model->index(row, VG_TABLE_COL_RELEASE).data().toInt();
    }

    this->markVoicegroupDirty(m_current_voicegroup_name);
    if (m_sample_delegate) {
        m_sample_delegate->setDirectSoundLabels(directSoundLabelsFromVoicegroups(m_edit_voicegroups));
    }
    this->updateStatusForCurrentVoicegroup();
}

void VoicegroupEditor::onKeysplitModelDataChanged(const QModelIndex &top_left,
                                                  const QModelIndex &bottom_right,
                                                  const QList<int> &) {
    KeysplitTable *table = this->currentEditableKeysplit();
    if (!table || !m_keysplit_model) {
        return;
    }

    for (int row = top_left.row(); row <= bottom_right.row(); ++row) {
        const int note = m_keysplit_model->index(row, 0).data().toInt();
        const int program = m_keysplit_model->index(row, 1).data().toInt();
        table->note_map[note] = program;
    }
    this->markKeysplitDirty(m_current_keysplit_name);
    this->updateStatusForCurrentKeysplit();
}

void VoicegroupEditor::onEntryFilterToggled(bool) {
    this->applyEntryFilters();
}

void VoicegroupEditor::onNewVoicegroupClicked() {
    if (!m_voicegroup_model) {
        return;
    }

    int index = m_edit_voicegroups.size() + 1;
    QString name = QString("voicegroup_new_%1").arg(index);
    while (m_edit_voicegroups.contains(name)) {
        ++index;
        name = QString("voicegroup_new_%1").arg(index);
    }
    m_edit_voicegroups.insert(name, VoiceGroup());
    if (m_sample_delegate) {
        m_sample_delegate->setVoicegroupLabels(m_edit_voicegroups.keys());
    }
    this->markVoicegroupDirty(name);
    this->rebuildVoicegroupListModel();
    this->updateStatusText(QString("Created %1").arg(name));
}

void VoicegroupEditor::onDuplicateVoicegroupClicked() {
    if (!m_voicegroup_model) {
        return;
    }
    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0) {
        return;
    }
    const QModelIndex idx = m_voicegroup_model->index(row, 0);
    const EntityKind kind = static_cast<EntityKind>(idx.data(k_role_entity_kind).toInt());
    if (kind != EntityKind::Voicegroup) {
        return;
    }
    const QString base = idx.data(k_role_entity_name).toString();
    if (!m_edit_voicegroups.contains(base)) {
        return;
    }

    QString name = QString("%1_copy").arg(base);
    int suffix = 2;
    while (m_edit_voicegroups.contains(name)) {
        name = QString("%1_copy_%2").arg(base, QString::number(suffix));
        ++suffix;
    }

    m_edit_voicegroups.insert(name, m_edit_voicegroups.value(base));
    if (m_sample_delegate) {
        m_sample_delegate->setVoicegroupLabels(m_edit_voicegroups.keys());
        m_sample_delegate->setDirectSoundLabels(directSoundLabelsFromVoicegroups(m_edit_voicegroups));
    }
    this->markVoicegroupDirty(name);
    this->rebuildVoicegroupListModel();
    this->updateStatusText(QString("Duplicated %1").arg(base));
}

void VoicegroupEditor::onRenameVoicegroupClicked() {
    if (!m_voicegroup_model) {
        return;
    }
    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0) {
        return;
    }

    const QModelIndex idx = m_voicegroup_model->index(row, 0);
    const EntityKind kind = static_cast<EntityKind>(idx.data(k_role_entity_kind).toInt());
    if (kind != EntityKind::Voicegroup) {
        return;
    }
    const QString old_name = idx.data(k_role_entity_name).toString();
    bool ok = false;
    const QString new_name = QInputDialog::getText(this, "Rename Voicegroup",
                                                   "New name:", QLineEdit::Normal,
                                                   old_name, &ok);
    if (!ok || new_name.trimmed().isEmpty()) {
        return;
    }

    const QString renamed = new_name.trimmed();
    if (!this->renameVoicegroupInEditModel(old_name, renamed)) {
        this->updateStatusText("Rename failed.");
        return;
    }
    if (m_sample_delegate) {
        m_sample_delegate->setVoicegroupLabels(m_edit_voicegroups.keys());
    }
    this->markVoicegroupDirty(renamed);
    this->rebuildVoicegroupListModel();
    this->updateStatusText(QString("Renamed %1").arg(renamed));
}

void VoicegroupEditor::onDeleteVoicegroupClicked() {
    if (!m_voicegroup_model) {
        return;
    }

    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0) {
        return;
    }

    const QModelIndex idx = m_voicegroup_model->index(row, 0);
    const EntityKind kind = static_cast<EntityKind>(idx.data(k_role_entity_kind).toInt());
    const QString name = idx.data(k_role_entity_name).toString();
    if (kind == EntityKind::Voicegroup) {
        m_edit_voicegroups.remove(name);
        m_dirty_voicegroups.remove(name);
    } else {
        m_edit_keysplit_tables.remove(name);
        m_dirty_keysplits.remove(name);
    }
    if (m_sample_delegate) {
        m_sample_delegate->setVoicegroupLabels(m_edit_voicegroups.keys());
        m_sample_delegate->setDirectSoundLabels(directSoundLabelsFromVoicegroups(m_edit_voicegroups));
    }
    if (m_current_voicegroup_name == name) {
        m_current_voicegroup_name.clear();
    }
    if (m_current_keysplit_name == name) {
        m_current_keysplit_name.clear();
    }
    this->rebuildVoicegroupListModel();
    this->updateStatusText(QString("Deleted %1").arg(name));
}

void VoicegroupEditor::onSaveVoicegroupClicked() {
    this->updateStatusText("Save is not wired yet.");
}

void VoicegroupEditor::onValidateVoicegroupClicked() {
    this->updateStatusText("Validate is not wired yet.");
}

void VoicegroupEditor::onFillRangeClicked() {
    this->updateStatusText("Fill range");
}

void VoicegroupEditor::onReplaceTypeClicked() {
    this->updateStatusText("Replace type");
}

void VoicegroupEditor::setVoicegroups(const QMap<QString, VoiceGroup> &voicegroups) {
    m_source_voicegroups = voicegroups;
    m_edit_voicegroups = voicegroups;
    m_dirty_voicegroups.clear();
    m_current_voicegroup_name.clear();
    if (m_sample_delegate) {
        m_sample_delegate->setDirectSoundLabels(directSoundLabelsFromVoicegroups(m_edit_voicegroups));
        m_sample_delegate->setVoicegroupLabels(m_edit_voicegroups.keys());
    }
    this->rebuildVoicegroupListModel();
}

void VoicegroupEditor::setKeysplitTables(const QMap<QString, KeysplitTable> &keysplit_tables) {
    m_source_keysplit_tables = keysplit_tables;
    m_edit_keysplit_tables = keysplit_tables;
    m_dirty_keysplits.clear();
    m_current_keysplit_name.clear();
    this->rebuildVoicegroupListModel();
}

void VoicegroupEditor::rebuildVoicegroupListModel() {
    if (!m_voicegroup_model) {
        return;
    }

    m_voicegroup_model->clear();
    for (auto it = m_edit_voicegroups.constBegin(); it != m_edit_voicegroups.constEnd(); ++it) {
        auto *item = new QStandardItem(QString("VG  %1").arg(it.key()));
        item->setData(static_cast<int>(EntityKind::Voicegroup), k_role_entity_kind);
        item->setData(it.key(), k_role_entity_name);
        m_voicegroup_model->appendRow(item);
    }
    for (auto it = m_edit_keysplit_tables.constBegin(); it != m_edit_keysplit_tables.constEnd(); ++it) {
        auto *item = new QStandardItem(QString("KS  %1").arg(it.key()));
        item->setData(static_cast<int>(EntityKind::Keysplit), k_role_entity_kind);
        item->setData(it.key(), k_role_entity_name);
        m_voicegroup_model->appendRow(item);
    }

    if (m_voicegroup_model->rowCount() <= 0) {
        this->updateStatusText("No voicegroups or keysplits loaded.");
        if (m_entry_model) {
            m_entry_model->removeRows(0, m_entry_model->rowCount());
        }
        return;
    }

    this->selectDefaultVoicegroup();
}

void VoicegroupEditor::selectDefaultVoicegroup() {
    if (!ui->listView_Voicegroups || !m_voicegroup_filter_model) {
        return;
    }
    if (m_voicegroup_filter_model->rowCount() <= 0) {
        return;
    }

    const QModelIndex idx = m_voicegroup_filter_model->index(0, 0);
    ui->listView_Voicegroups->setCurrentIndex(idx);
}

/**
 * Rebuild the entries table from the currently selected editable voicegroup.
 * Also initializes table selection and inspector values for the new context.
 */
void VoicegroupEditor::refreshEntryTableForSelectedVoicegroup() {
    if (!m_entry_model) {
        return;
    }
    if (ui->tableView_VoicegroupEntries) {
        ui->tableView_VoicegroupEntries->show();
    }
    if (m_keysplit_table_view) {
        m_keysplit_table_view->hide();
    }

    m_entry_model->removeRows(0, m_entry_model->rowCount());

    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0 || !m_voicegroup_model) {
        m_current_voicegroup_name.clear();
        this->updateStatusText("No voicegroup selected.");
        return;
    }
    const QModelIndex list_idx = m_voicegroup_model->index(row, 0);
    const QString selected_name = list_idx.data(k_role_entity_name).toString();
    m_current_voicegroup_name = selected_name;
    m_current_keysplit_name.clear();
    const auto vg_it = m_edit_voicegroups.constFind(selected_name);
    if (vg_it == m_edit_voicegroups.constEnd()) {
        this->updateStatusText("Selected voicegroup was not found.");
        return;
    }
    const VoiceGroup &group = vg_it.value();

    for (int i = 0; i < group.instruments.size(); ++i) {
        const Instrument &inst = group.instruments[i];
        const QString type = displayTypeName(inst);
        const QString sample = inst.sample_label;
        const int key = inst.base_key;
        const int pan = inst.pan;
        const int attack = inst.attack;
        const int decay = inst.decay;
        const int sustain = inst.sustain;
        const int release = inst.release;
        const QString flags = inst.keysplit_table.isEmpty() ? "" : inst.keysplit_table;

        QList<QStandardItem *> row_items;
        row_items.reserve(VG_TABLE_COL_COUNT);
        row_items.append(new QStandardItem(QString::number(i)));
        row_items.append(new QStandardItem(type));
        row_items.append(new QStandardItem(sample));
        row_items.append(new QStandardItem(QString::number(key)));
        row_items.append(new QStandardItem(QString::number(pan)));
        row_items.append(new QStandardItem(QString::number(attack)));
        row_items.append(new QStandardItem(QString::number(decay)));
        row_items.append(new QStandardItem(QString::number(sustain)));
        row_items.append(new QStandardItem(QString::number(release)));
        row_items.append(new QStandardItem(flags));

        row_items[VG_TABLE_COL_INDEX]->setEditable(false);
        row_items[VG_TABLE_COL_TYPE]->setEditable(true);
        row_items[VG_TABLE_COL_SAMPLE]->setEditable(true);
        row_items[VG_TABLE_COL_KEY]->setEditable(true);
        row_items[VG_TABLE_COL_PAN]->setEditable(true);
        row_items[VG_TABLE_COL_ATTACK]->setEditable(true);
        row_items[VG_TABLE_COL_DECAY]->setEditable(true);
        row_items[VG_TABLE_COL_SUSTAIN]->setEditable(true);
        row_items[VG_TABLE_COL_RELEASE]->setEditable(true);
        row_items[VG_TABLE_COL_FLAGS]->setEditable(true);

        row_items[VG_TABLE_COL_INDEX]->setData(true, k_role_is_used);
        row_items[VG_TABLE_COL_INDEX]->setData(false, k_role_has_error);
        m_entry_model->appendRow(row_items);
    }

    this->applyEntryFilters();

    if (ui->tableView_VoicegroupEntries && ui->tableView_VoicegroupEntries->selectionModel()
        && m_entry_model->rowCount() > 0) {
        const QModelIndex first = m_entry_model->index(0, 0);
        ui->tableView_VoicegroupEntries->selectionModel()->setCurrentIndex(
            first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
    }

    this->updateStatusForCurrentVoicegroup();
}

void VoicegroupEditor::refreshKeysplitTableForSelectedKeysplit() {
    if (!m_keysplit_model || !m_voicegroup_model) {
        return;
    }
    if (ui->tableView_VoicegroupEntries) {
        ui->tableView_VoicegroupEntries->hide();
    }
    if (m_keysplit_table_view) {
        m_keysplit_table_view->show();
    }
    m_keysplit_model->removeRows(0, m_keysplit_model->rowCount());

    const int row = this->selectedVoicegroupSourceRow();
    if (row < 0) {
        m_current_keysplit_name.clear();
        this->updateStatusText("No keysplit selected.");
        return;
    }

    const QModelIndex list_idx = m_voicegroup_model->index(row, 0);
    const QString selected_name = list_idx.data(k_role_entity_name).toString();
    m_current_keysplit_name = selected_name;
    m_current_voicegroup_name.clear();

    const auto it = m_edit_keysplit_tables.constFind(selected_name);
    if (it == m_edit_keysplit_tables.constEnd()) {
        this->updateStatusText("Selected keysplit was not found.");
        return;
    }

    const KeysplitTable &table = it.value();
    for (auto map_it = table.note_map.constBegin(); map_it != table.note_map.constEnd(); ++map_it) {
        QList<QStandardItem *> row_items;
        row_items.append(new QStandardItem(QString::number(map_it.key())));
        row_items.append(new QStandardItem(QString::number(map_it.value())));
        m_keysplit_model->appendRow(row_items);
    }

    this->updateStatusForCurrentKeysplit();
}

void VoicegroupEditor::applyEntryFilters() {
    if (!ui->tableView_VoicegroupEntries || !m_entry_model) {
        return;
    }

    const bool unique_only = ui->checkBox_ShowUsedOnly && ui->checkBox_ShowUsedOnly->isChecked();
    const bool errors_only = ui->checkBox_ShowErrorsOnly && ui->checkBox_ShowErrorsOnly->isChecked();
    QSet<QString> seen_rows;

    for (int row = 0; row < m_entry_model->rowCount(); ++row) {
        const QModelIndex idx = m_entry_model->index(row, VG_TABLE_COL_INDEX);
        const bool has_error = idx.data(k_role_has_error).toBool();

        const QString row_signature = QString("%1|%2|%3|%4|%5|%6|%7|%8|%9")
            .arg(m_entry_model->index(row, VG_TABLE_COL_TYPE).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_SAMPLE).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_KEY).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_PAN).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_ATTACK).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_DECAY).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_SUSTAIN).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_RELEASE).data().toString())
            .arg(m_entry_model->index(row, VG_TABLE_COL_FLAGS).data().toString());

        bool hide_row = false;
        if (unique_only) {
            if (seen_rows.contains(row_signature)) {
                hide_row = true;
            } else {
                seen_rows.insert(row_signature);
            }
        }
        if (errors_only && !has_error) {
            hide_row = true;
        }

        ui->tableView_VoicegroupEntries->setRowHidden(row, hide_row);
    }
}

void VoicegroupEditor::updateStatusText(const QString &text) {
    if (ui->label_Status) {
        ui->label_Status->setText(text);
    }
}

void VoicegroupEditor::updateStatusForCurrentVoicegroup() {
    if (m_current_voicegroup_name.isEmpty()) {
        this->updateStatusText("No voicegroup selected.");
        return;
    }

    const VoiceGroup *group = this->currentEditableVoicegroup();
    if (!group) {
        this->updateStatusText("No voicegroup selected.");
        return;
    }

    const bool dirty = m_dirty_voicegroups.contains(m_current_voicegroup_name);
    const QString dirty_text = dirty ? "dirty" : "clean";
    this->updateStatusText(
        QString("%1 (%2 entries, %3)")
            .arg(m_current_voicegroup_name, QString::number(group->instruments.size()), dirty_text)
    );
}

void VoicegroupEditor::updateStatusForCurrentKeysplit() {
    if (m_current_keysplit_name.isEmpty()) {
        this->updateStatusText("No keysplit selected.");
        return;
    }

    const KeysplitTable *table = this->currentEditableKeysplit();
    if (!table) {
        this->updateStatusText("No keysplit selected.");
        return;
    }

    const bool dirty = m_dirty_keysplits.contains(m_current_keysplit_name);
    const QString dirty_text = dirty ? "dirty" : "clean";
    this->updateStatusText(
        QString("%1 (%2 mappings, %3)")
            .arg(m_current_keysplit_name, QString::number(table->note_map.size()), dirty_text)
    );
}

void VoicegroupEditor::markVoicegroupDirty(const QString &voicegroup) {
    if (voicegroup.isEmpty()) {
        return;
    }
    m_dirty_voicegroups.insert(voicegroup);
}

void VoicegroupEditor::markKeysplitDirty(const QString &keysplit) {
    if (keysplit.isEmpty()) {
        return;
    }
    m_dirty_keysplits.insert(keysplit);
}

bool VoicegroupEditor::renameVoicegroupInEditModel(const QString &old_name, const QString &new_name) {
    if (old_name.isEmpty() || new_name.isEmpty() || old_name == new_name) {
        return false;
    }
    if (!m_edit_voicegroups.contains(old_name) || m_edit_voicegroups.contains(new_name)) {
        return false;
    }

    VoiceGroup group = m_edit_voicegroups.take(old_name);
    m_edit_voicegroups.insert(new_name, group);

    if (m_dirty_voicegroups.remove(old_name) > 0) {
        m_dirty_voicegroups.insert(new_name);
    }
    if (m_current_voicegroup_name == old_name) {
        m_current_voicegroup_name = new_name;
    }
    return true;
}

VoiceGroup *VoicegroupEditor::currentEditableVoicegroup() {
    if (m_current_voicegroup_name.isEmpty()) {
        return nullptr;
    }
    auto it = m_edit_voicegroups.find(m_current_voicegroup_name);
    if (it == m_edit_voicegroups.end()) {
        return nullptr;
    }
    return &it.value();
}

const VoiceGroup *VoicegroupEditor::currentEditableVoicegroup() const {
    if (m_current_voicegroup_name.isEmpty()) {
        return nullptr;
    }
    auto it = m_edit_voicegroups.constFind(m_current_voicegroup_name);
    if (it == m_edit_voicegroups.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

KeysplitTable *VoicegroupEditor::currentEditableKeysplit() {
    if (m_current_keysplit_name.isEmpty()) {
        return nullptr;
    }
    auto it = m_edit_keysplit_tables.find(m_current_keysplit_name);
    if (it == m_edit_keysplit_tables.end()) {
        return nullptr;
    }
    return &it.value();
}

const KeysplitTable *VoicegroupEditor::currentEditableKeysplit() const {
    if (m_current_keysplit_name.isEmpty()) {
        return nullptr;
    }
    auto it = m_edit_keysplit_tables.constFind(m_current_keysplit_name);
    if (it == m_edit_keysplit_tables.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

int VoicegroupEditor::selectedVoicegroupSourceRow() const {
    if (!ui->listView_Voicegroups || !m_voicegroup_filter_model) {
        return -1;
    }
    const QModelIndex proxy_idx = ui->listView_Voicegroups->currentIndex();
    if (!proxy_idx.isValid()) {
        return -1;
    }
    const QModelIndex source_idx = m_voicegroup_filter_model->mapToSource(proxy_idx);
    return source_idx.isValid() ? source_idx.row() : -1;
}
