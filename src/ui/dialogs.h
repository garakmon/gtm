#pragma once
#ifndef DIALOGS_H
#define DIALOGS_H

#include "app/structs.h"

#include <QDialog>
#include <QSet>
#include <QString>
#include <QStringList>



class GTMLineEdit;
class GTMPrefixedLineEdit;
class GTMComboBox;
class GTMSpinBox;
class GTMFormLayout;
class QDialogButtonBox;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// NewSongDialog is the pop up window for users to create new songs. It allows for 
/// minimal customization at song creation, including assigning a voicegroup and title.
///
//////////////////////////////////////////////////////////////////////////////////////////
class NewSongDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewSongDialog(QWidget *parent = nullptr);
    ~NewSongDialog() override = default;

    NewSongDialog(const NewSongDialog &) = delete;
    NewSongDialog &operator=(const NewSongDialog &) = delete;
    NewSongDialog(NewSongDialog &&) = delete;
    NewSongDialog &operator=(NewSongDialog &&) = delete;

    NewSongSettings result() const;
    void setVoicegroups(const QStringList &voicegroups);
    void setPlayers(const QStringList &players);
    void setExistingSongTitles(const QStringList &titles);

private:
    void setup();
    void updateAutoFields();
    QString nextUniqueSongTitle() const;
    void onAccepted();
    bool validate(QString *error) const;
    bool isTitleUnique(const QString &title) const;

private:
    QSet<QString> m_existing_song_titles; // to make sure title is unique

    GTMFormLayout *m_form = nullptr;
    QDialogButtonBox *m_button_box = nullptr;
    GTMPrefixedLineEdit *m_input_title = nullptr;
    GTMLineEdit *m_auto_const = nullptr;
    GTMLineEdit *m_auto_midi_path = nullptr;
    GTMComboBox *m_input_voicegroup = nullptr;
    GTMComboBox *m_input_player = nullptr;
    GTMSpinBox *m_input_priority = nullptr;
    GTMSpinBox *m_input_reverb = nullptr;
    GTMSpinBox *m_input_volume = nullptr;
};

#endif // DIALOGS_H
