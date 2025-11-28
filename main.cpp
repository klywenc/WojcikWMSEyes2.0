#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);


    int id1 = QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular.ttf");
    int id2 = QFontDatabase::addApplicationFont(":/fonts/RobotoCondensed-Bold.ttf");

    if (id1 == -1 || id2 == -1) {
        qWarning() << "Blad ladowania czcionek! Upewnij sie, ze pliki sa w folderze fonts i dodane do resources.qrc";
    }

    QFont font("Roboto", 10);
    a.setFont(font);

    MainWindow w;
    w.setWindowTitle("WMS Eyes 2.0");
    w.resize(1280, 900);
    w.show();

    return a.exec();
}