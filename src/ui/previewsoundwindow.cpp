#include "ui/previewsoundwindow.h"
#include "ui_previewsoundwindow.h"



PreviewSoundWindow::PreviewSoundWindow(QWidget *parent)
  : QDialog(parent) , ui(new Ui::PreviewSoundWindow) {
    ui->setupUi(this);
    this->setupUi();
}

PreviewSoundWindow::~PreviewSoundWindow() {
    delete ui;
}

void PreviewSoundWindow::setupUi() {
    //
}

void PreviewSoundWindow::loadSound() {
    //
}
