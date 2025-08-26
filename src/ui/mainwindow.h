#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QFileDialog>



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

    PianoRoll *m_piano_roll = nullptr; // TODO: unique_ptr? maybe break with ownership

private:
    void setupUi();

    void drawScoreArea();
    void drawScoreAreaGrid();

    QString browse(QString filter, QFileDialog::FileMode mode);
    void open(QString path);

private slots:
    // void on_action_open_triggered();
};
#endif // MAINWINDOW_H
