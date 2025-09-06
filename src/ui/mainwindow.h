#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QFileDialog>
#include <memory>

#include "project.h"



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class PianoRoll;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    std::unique_ptr<Project> m_project;

    PianoRoll *m_piano_roll = nullptr; // TODO: unique_ptr? maybe break with ownership of qobjects
    QGraphicsScene *m_scene_tracks = nullptr;

private:
    void setupUi();
    void loadProject();

    void loadSong();

    void drawScoreArea();
    void drawTrackList();

    QString browse(QString filter, QFileDialog::FileMode mode);
    void open(QString path);

private slots:
    // void on_action_open_triggered();
};
#endif // MAINWINDOW_H
