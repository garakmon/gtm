#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    MainWindow w;
    w.show();
    return a.exec();
}
