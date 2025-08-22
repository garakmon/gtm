#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsEllipseItem>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // silence bug in my qt version re: mac trackpads
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));
}

MainWindow::~MainWindow() {
    delete scene_score;
    delete ui;
}

void MainWindow::setupUi() {

    // alloc new scenes
    scene_score = new QGraphicsScene;

    drawScoreArea();
}

void MainWindow::drawScoreArea() {
    // score area only redrawn when changes, otherwise translating the area based on measure, (virtual pixels? or scroll down too?)
    drawScoreAreaGrid();
}

void MainWindow::drawScoreAreaGrid() {
    QGraphicsView *view = this->ui->graphicsView_PianoRoll;
    view->setBackgroundBrush(QBrush(QColor(209, 230, 229), Qt::SolidPattern));

    // QPen pen(QColor(150, 150, 150), 0);

    view->setScene(scene_score);
}

void MainWindow::open(QString path) {
    qDebug() << "OPEN";
}

QString MainWindow::browse(QString filter, QFileDialog::FileMode mode) {
    QFileDialog browser;
    browser.setFileMode(mode);
    QString filepath = browser.getOpenFileName(this, "Select a File", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), filter);
    return filepath;
}

// void MainWindow::on_action_open_triggered() {
//     QString path = browse("Midi Files (*.mid *.midi *.MID *.MIDI)", QFileDialog::ExistingFile);
//     open(path);
// }


