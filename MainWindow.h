#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QShortcut>
#include <QSerialPort>
#include <opencv2/opencv.hpp>
#include <QtNetwork>
#include <QQueue>
#include <QThreadPool>
#include <QRunnable>
#include <QMutex>
#include <QMap>
#include "SettingDialog.h"

struct UploadJob {
    QString filePath;
    QString palletCode;
    int camIndex;
};

class CameraWorker final : public QObject, public QRunnable {
    Q_OBJECT
public:
    CameraWorker(int index, QString url, int protocolMode, int rotation,
                 QString user, QString pass, QString savePath, QObject *parent = nullptr);
    void run() override;

signals:
    void resultReady(int index, bool success, QString filePath, QString errorMsg);

private:
    int m_index;
    QString m_url;
    int m_protocol;
    int m_rotation;
    QString m_user;
    QString m_pass;
    QString m_savePath;
};

class UploadWorker : public QObject {
    Q_OBJECT
public:
    explicit UploadWorker(QString serverUrl, int timeout, QObject *parent = nullptr);

public slots:
    void addJob(const UploadJob& job);
    void processNext();

signals:
    void uploadStarted(int camIndex);
    void uploadFinished(int camIndex, bool success, QString message);

private:
    void sendRequest(const UploadJob &job);

    QNetworkAccessManager *manager;
    QQueue<UploadJob> m_queue;
    bool m_isUploading;
    QString m_serverUrl;
    int m_timeout;
};

// --- GŁÓWNE OKNO ---
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void startScanProcess();
    void openSettings();
    void handleSerialScan();
    void updateClock() const;

    void onCameraFinished(int index, bool success, const QString& filePath, const QString& errorMsg);
    void onWorkerUploadStarted(int camIndex);
    void onWorkerUploadFinished(int camIndex, bool success, const QString& message);

signals:
    void requestUpload(UploadJob job);

private:
    void setupUi();
    void setupStyles();
    void configureScanner();
    static void ensureTmpFolderExists();
    static QLabel* createCameraLabel(const QString &text);

    // Status (0=Idle, 1=Sending, 2=OK, 3=Error)
    void drawStatusOnImage(int camIndex, const QString &filePath, int status, const QString &msg = "");

    QWidget *centralWidget;
    QWidget *mainPanel;
    QLabel *logoLabel;
    QLabel *headerTitle;
    QLabel *dateLabel;
    QLabel *cam1_TopLeft; QLabel *cam4_TopRight; QLabel *cam0_BotLeft; QLabel *cam2_BotMid; QLabel *cam3_BotRight;
    std::vector<QLabel*> camDisplays;

    SettingDialog *settingsDialog;
    QShortcut *secretShortcut;
    QShortcut *exitShortcut;
    QTimer *clockTimer;

    QSerialPort *serialScanner;
    QByteArray serialBuffer;
    QString currentPalletCode;

    QString lastImagePaths[5];

    QThreadPool *cameraPool;
    QThread *uploadThread;
    UploadWorker *uploadWorker;
};

#endif
