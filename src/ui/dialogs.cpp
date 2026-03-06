#include "ui/dialogs.h"

#include "ui/customwidgets.h"

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>



NewSongDialog::NewSongDialog(QWidget *parent) : QDialog(parent) {
    this->setup();
}

void NewSongDialog::setup() {
    this->setWindowTitle("New Song Creator");

    m_form = new GTMFormLayout(this);

    // widget inputs
    m_input_title = new GTMPrefixedLineEdit("mus_", this);
    m_auto_const = new GTMLineEdit(this);
    m_auto_midi_path = new GTMLineEdit(this);
    m_input_voicegroup = new GTMComboBox(this);
    m_input_player = new GTMComboBox(this);
    m_input_player->setEditable(false);
    m_input_priority = new GTMSpinBox(this);
    m_input_reverb = new GTMSpinBox(this);
    m_input_volume = new GTMSpinBox(this);

    // set valid ranges and preset values
    m_auto_const->setReadOnly(true);
    m_auto_midi_path->setReadOnly(true);
    m_input_voicegroup->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_input_player->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_input_priority->setRange(0, 255);
    m_input_reverb->setRange(0, 255);
    m_input_volume->setRange(0, 127);
    m_input_priority->setValue(0);
    m_input_reverb->setValue(0);
    m_input_volume->setValue(127);
    m_input_title->setValidator(
        new QRegularExpressionValidator(QRegularExpression("mus_[a-z0-9_]*"), m_input_title)
    );
    m_input_title->setText("mus_newsong_1");

    this->setPlayers({"MUS_PLAYER"});
    connect(m_input_title, &QLineEdit::textChanged, this, [this](const QString &) {
        // live updates on filepath and song constant
        this->updateAutoFields();
    });

    // add rows to form
    m_form->addRow("Song Title", m_input_title);
    m_form->addRow("Song Constant", m_auto_const);
    m_form->addRow("Midi Path", m_auto_midi_path);
    m_form->addRow("Voicegroup", m_input_voicegroup);
    m_form->addRow("Player", m_input_player);
    m_form->addRow("Priority", m_input_priority);
    m_form->addRow("Reverb", m_input_reverb);
    m_form->addRow("Volume", m_input_volume);

    m_button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this
    );
    connect(m_button_box, &QDialogButtonBox::accepted, this, &NewSongDialog::onAccepted);
    connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_form->addRow(m_button_box);

    this->updateAutoFields();
}

/**
 * Return a struct with data from the user's inputs so the Controller can create the song.
 */
NewSongSettings NewSongDialog::result() const {
    NewSongSettings settings;
    settings.title = m_input_title ? m_input_title->text() : QString();
    settings.constant = m_auto_const ? m_auto_const->text() : QString();
    settings.midi_path = m_auto_midi_path ? m_auto_midi_path->text() : QString();
    settings.voicegroup = m_input_voicegroup ? m_input_voicegroup->currentText() : QString();
    settings.player = m_input_player ? m_input_player->currentText() : QString();
    settings.priority = m_input_priority ? m_input_priority->value() : 0;
    settings.reverb = m_input_reverb ? m_input_reverb->value() : 0;
    settings.volume = m_input_volume ? m_input_volume->value() : 0;
    return settings;
}

void NewSongDialog::setVoicegroups(const QStringList &voicegroups) {
    if (!m_input_voicegroup) return;

    m_input_voicegroup->clear();
    m_input_voicegroup->addItems(voicegroups);
    if (m_form) {
        m_form->syncFieldWidths();
    }
}

void NewSongDialog::setPlayers(const QStringList &players) {
    if (!m_input_player) return;

    m_input_player->clear();
    m_input_player->addItems(players);
    if (m_input_player->findText("MUS_PLAYER") >= 0) {
        m_input_player->setCurrentText("MUS_PLAYER");
    } else if (m_input_player->count() > 0) {
        m_input_player->setCurrentIndex(0);
    }
    if (m_form) {
        m_form->syncFieldWidths();
    }
}

void NewSongDialog::setExistingSongTitles(const QStringList &titles) {
    m_existing_song_titles.clear();
    for (const QString &title : titles) {
        m_existing_song_titles.insert(title.toLower());
    }

    if (m_input_title) {
        const QString unique = this->nextUniqueSongTitle();
        QSignalBlocker blocker(m_input_title);
        m_input_title->setText(unique);
    }
    this->updateAutoFields();
}

void NewSongDialog::updateAutoFields() {
    const QString title = m_input_title ? m_input_title->text() : QString();
    if (!m_auto_const || !m_auto_midi_path) {
        return;
    }

    if (title.isEmpty()) {
        m_auto_const->clear();
        m_auto_midi_path->clear();
        return;
    }

    m_auto_const->setText(title.toUpper());
    m_auto_midi_path->setText("sound/songs/midi/" + title + ".mid");

    if (m_form) {
        m_form->syncFieldWidths();
    }
}

QString NewSongDialog::nextUniqueSongTitle() const {
    int index = 1;
    while (true) {
        const QString candidate = QString("mus_newsong_%1").arg(index);
        if (!m_existing_song_titles.contains(candidate)) {
            return candidate;
        }
        ++index;
    }
}

void NewSongDialog::onAccepted() {
    QString error;
    if (!this->validate(&error)) {
        QMessageBox::warning(this, "Invalid Song Settings", error);
        return;
    }
    this->accept();
}

bool NewSongDialog::validate(QString *error) const {
    const QString title = m_input_title ? m_input_title->text() : QString();
    if (title.size() <= 4) {
        // don't accept "mus_" as the title
        if (error) *error = "Song title must be longer than 4 characters.";
        return false;
    }
    if (!m_input_title || !m_input_title->hasAcceptableInput()) {
        if (error) *error = "Song title must match: mus_[a-z0-9_]*";
        return false;
    }
    if (!this->isTitleUnique(title)) {
        if (error) *error = QString("Song title '%1' already exists.").arg(title);
        return false;
    }
    return true;
}

bool NewSongDialog::isTitleUnique(const QString &title) const {
    if (title.isEmpty()) {
        return false;
    }
    return !m_existing_song_titles.contains(title.toLower());
}
