#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>
#include <QCloseEvent>
#include <QMainWindow>
#include <QPointer>
#include <QVector>

#include "ui/graphicstrackitem.h"
#include "util/gtmconfig.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Controller;
class PianoRoll;
class TrackRoll;
class PreviewSoundWindow;
class MinimapWidget;
class MasterMeterWidget;
class QComboBox;
class QSortFilterProxyModel;
class QToolButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setRecentSongTitle(const QString &title);

    friend class Controller;
private:
    Ui::MainWindow *ui;

    QPointer<PreviewSoundWindow> m_preview_sound_window;

    std::unique_ptr<Controller> m_controller;
    QPointer<MinimapWidget> m_minimap;
    QPointer<MasterMeterWidget> m_master_meter;
    QString m_project_root;
    GtmConfig m_config;
    QVector<int> m_splitter_sizes;
    QSortFilterProxyModel *m_song_filter = nullptr;
    QVector<QToolButton *> m_track_event_filter_buttons;
    uint32_t m_track_event_mask_ui = kTrackEventView_All;
    QComboBox *m_track_event_preset_combo = nullptr;

    // PianoRoll *m_piano_roll = nullptr; // TODO: unique_ptr? maybe break with ownership of qobjects
    // TrackRoll *m_track_roll = nullptr;

private:
    void setupUi();
    void loadProject();
    void syncSongListSelectionToOpenSong(bool scroll_to_center);
    void setupTrackMetaControls();
    void applyTrackMetaMaskFromUi();

    void loadSong();

    void drawTrackList();

    void open(QString path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void on_action_Open_triggered();
    void on_action_PreviewSound_triggered();
    void on_Button_Play_clicked();
    void on_Button_Pause_clicked();
    void on_Button_Stop_clicked();
    void on_Button_Previous_clicked();
    void on_Button_Skip_clicked();
};

#endif // MAINWINDOW_H
