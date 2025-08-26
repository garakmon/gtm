#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGraphicsEllipseItem>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>

#include "pianoroll.h"



MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setupUi();

    // TEMP: silence bug in my qt version re: mac trackpads [fix: update Qt version]
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));
}

MainWindow::~MainWindow() {
    // delete m_piano_roll;
    delete ui;
}

void MainWindow::setupUi() {

    // alloc new scenes
    m_piano_roll = new PianoRoll(this);
    this->ui->view_piano->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_piano->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->view_piano->setScene(m_piano_roll->scenePiano());
    // this->ui->view_piano->setSceneRect(m_piano_roll->scenePiano()->itemsBoundingRect());

    this->ui->view_piano->centerOn(m_piano_roll->keys()[60]);

    this->ui->view_piano->setBackgroundBrush(QBrush(QColor(209, 230, 229), Qt::SolidPattern));

    // drawScoreArea();
}

void MainWindow::drawScoreArea() {
    // score area only redrawn when changes, otherwise translating the area based on measure, (virtual pixels? or scroll down too?)
    drawScoreAreaGrid();
}

void MainWindow::drawScoreAreaGrid() {
    QGraphicsView *view = this->ui->view_pianoRoll;
    view->setBackgroundBrush(QBrush(QColor(209, 230, 229), Qt::SolidPattern));

    // QPen pen(QColor(150, 150, 150), 0);

    //view->setScene(scene_score);
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


