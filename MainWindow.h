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
#include "SettingDialog.h"

struct UploadJob {
    QString filePath;
    QString palletCode;
    int camIndex;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void captureSnapshot();
    void openSettings();
    void handleSerialScan();
    void startNextUpload();
    void onUploadFinished(QNetworkReply *reply);
    void updateUploadAnimation();

private:
    void setupUi();
    void setupStyles();
    void reloadCameras();
    void configureScanner();
    void ensureTmpFolderExists();

    QLabel* createCameraLabel(const QString &text);
    void drawStatusOnImage(int camIndex, const QPixmap &basePix, QString text, QColor color, bool isAnimated = false);

    QWidget *centralWidget;
    QWidget *mainPanel;
    QLabel *logoLabel;
    QLabel *headerTitle;
    QLabel *dateLabel;

    QLabel *cam1_TopLeft;
    QLabel *cam4_TopRight;
    QLabel *cam0_BotLeft;
    QLabel *cam2_BotMid;
    QLabel *cam3_BotRight;

    SettingDialog *settingsDialog;
    QShortcut *secretShortcut;
    QShortcut *exitShortcut;

    std::vector<cv::VideoCapture> captures;
    std::vector<QLabel*> camDisplays;
    QTimer *clockTimer;

    QSerialPort *serialScanner;
    QByteArray serialBuffer;

    QNetworkAccessManager *netManager;
    QQueue<UploadJob> uploadQueue;
    bool isUploading;
    QString currentPalletCode;

    QTimer *animationTimer;
    int spinnerAngle;
    QPixmap currentBasePixmap;
    int currentUploadCamIndex;
};

#endif