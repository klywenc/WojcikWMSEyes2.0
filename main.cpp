#include <QApplication>
#include <QFontDatabase>
#include <QDateTime>
#include <QThread>
#include <cstdio>
#include "MainWindow.h"

// Funkcja formatująca logi w konsoli
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    // Pobierz ID wątku (format 0x...)
    QString threadId = QString::number((quint64)QThread::currentThreadId(), 16);

    const char *color = "";
    const char *label = "";

    switch (type) {
    case QtDebugMsg:
        color = "\033[32m"; // Zielony
        label = "DBG";
        break;
    case QtInfoMsg:
        color = "\033[36m"; // Cyjan
        label = "INF";
        break;
    case QtWarningMsg:
        color = "\033[33m"; // Żółty
        label = "WRN";
        break;
    case QtCriticalMsg:
        color = "\033[31m"; // Czerwony
        label = "ERR";
        break;
    case QtFatalMsg:
        color = "\033[41m"; // Czerwone tło
        label = "FTL";
        break;
    }

    fprintf(stderr, "%s[%s] [Thread: %s] %s: %s\033[0m\n",
            color, qPrintable(timeStr), qPrintable(threadId), label, localMsg.constData());
    fflush(stderr);
}

int main(int argc, char *argv[]) {
    // Instalacja handlera logów przed startem aplikacji
    qInstallMessageHandler(customMessageHandler);

    QApplication a(argc, argv);

    qDebug() << ">>> SYSTEM STARTUP <<<";
    qDebug() << "Loading fonts...";

    if(QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular.ttf") == -1)
        qWarning() << "Failed to load Roboto-Regular.ttf";
    if(QFontDatabase::addApplicationFont(":/fonts/RobotoCondensed-Bold.ttf") == -1)
        qWarning() << "Failed to load RobotoCondensed-Bold.ttf";

    QFont font("Roboto", 10);
    a.setFont(font);

    qDebug() << "Initializing MainWindow...";
    MainWindow w;

    qDebug() << "Entering Event Loop...";
    return a.exec();
}
