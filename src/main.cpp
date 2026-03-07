#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>



int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(":/fonts/IBMPlexMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/IBMPlexSans-Regular.ttf");
    a.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    MainWindow w;
    w.show();
    return a.exec();
}
