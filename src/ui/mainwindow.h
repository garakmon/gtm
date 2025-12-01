#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QFileDialog>
#include <QCloseEvent>
#include <memory>

#include "../app/project.h"
#include "../app/controller.h"
#include "../util/gtmconfig.h"



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class PianoRoll;
class TrackRoll;
class PreviewSoundWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

friend class Controller;
private:
    Ui::MainWindow *ui;

    QPointer<PreviewSoundWindow> m_preview_sound_window;

    std::unique_ptr<Controller> m_controller;
    QString m_project_root;
    GtmConfig m_config;

    // PianoRoll *m_piano_roll = nullptr; // TODO: unique_ptr? maybe break with ownership of qobjects
    // TrackRoll *m_track_roll = nullptr;

private:
    void setupUi();
    void loadProject();

    void loadSong();

    void drawTrackList();

    void open(QString path);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_action_Open_triggered();
    void on_action_PreviewSound_triggered();
    void on_Button_Play_clicked();
    void on_Button_Stop_clicked();
};

#endif // MAINWINDOW_H
