#pragma once

#ifndef PREVIEWSOUNDWINDOW_H
#define PREVIEWSOUNDWINDOW_H

#include <QDialog>



QT_BEGIN_NAMESPACE
namespace Ui { class PreviewSoundWindow; }
QT_END_NAMESPACE

class PreviewSoundWindow : public QDialog {
    Q_OBJECT

public:
    PreviewSoundWindow(QWidget *parent = nullptr);
    ~PreviewSoundWindow();

private:
    Ui::PreviewSoundWindow *ui;

private:
    void setupUi();
    void loadSound();

    void play() {}

private slots:
    // void on_action_open_triggered();
};



#endif // PREVIEWSOUNDWINDOW_H
