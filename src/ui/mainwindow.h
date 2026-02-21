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



namespace Ui { class MainWindow; }
class Controller;
class PianoRoll;
class TrackRoll;
class PreviewSoundWindow;
class MinimapWidget;
class MasterMeterWidget;
class QComboBox;
class QSortFilterProxyModel;
class QToolButton;

//////////////////////////////////////////////////////////////////////////////////////////
///
/// The MainWindow is the class for the application window.
/// 
/// Its function is to manage the widget tree and layouts within the Ui::MainWindow.
/// Additionally, it manages user inputs on said ui and forwards them to the Controller.
///
//////////////////////////////////////////////////////////////////////////////////////////
class MainWindow : public QMainWindow {
    Q_OBJECT

public: // methods
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // disable copy and move ops
    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;
    MainWindow(MainWindow &&) = delete;
    MainWindow &operator=(MainWindow &&) = delete;

    friend class Controller;

private: // methods
    // window initialization
    void setupUi();
    void loadConfig();
    void setupController();
    void setupEventEditorRanges();
    void setupMinimap();
    void setupMasterMeter();
    void setupSongSearch();
    void setupToolButtons();
    void setupMetaDisplay();
    void setupSplitterControls();

    // project load realted functions
    void loadProject();
    void loadProject(const QString &root);

    // ui updates
    void syncSongListSelectionToOpenSong(bool scroll_to_center);
    void setRecentSongTitle(const QString &title);
    void setupTrackMetaControls();
    void applyTrackMetaMaskFromUi();

protected: // methods
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // toolbar menu actions
    void on_action_Open_triggered();

    // player control actions
    void on_Button_Play_clicked();
    void on_Button_Pause_clicked();
    void on_Button_Stop_clicked();
    void on_Button_Previous_clicked();
    void on_Button_Skip_clicked();

    // other actions
    void on_action_PreviewSound_triggered();

private: // members
    Ui::MainWindow *ui;

    std::unique_ptr<Controller> m_controller;
    GtmConfig m_config;

    // subwindows
    QPointer<PreviewSoundWindow> m_preview_sound_window;

    // manually initialized ui elements
    QPointer<MinimapWidget> m_minimap;
    QPointer<MasterMeterWidget> m_master_meter;
    QPointer<QSortFilterProxyModel> m_song_filter;
    QVector<QPointer<QToolButton>> m_track_event_filter_buttons;
    QPointer<QComboBox> m_track_event_preset_combo;

    // state tracking variables
    uint32_t m_track_event_mask_ui = TrackEventView_All;
    QVector<int> m_splitter_sizes;
};

#endif // MAINWINDOW_H
