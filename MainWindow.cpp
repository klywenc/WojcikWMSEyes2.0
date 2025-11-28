#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QMessageBox>
#include <QKeyEvent>
#include <QApplication>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QHttpMultiPart>
#include <QUrlQuery>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    this->setObjectName("mainWindow");

    qDebug() << "Application starting...";

    // Stabilizacja RTSP
    qputenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp");

    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    settingsDialog = new SettingDialog(this);
    serialScanner = new QSerialPort(this);

    netManager = new QNetworkAccessManager(this);
    isUploading = false;

    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MainWindow::updateUploadAnimation);
    spinnerAngle = 0;

    ensureTmpFolderExists();
    setupStyles();
    setupUi();

    secretShortcut = new QShortcut(QKeySequence("Ctrl+5"), this);
    connect(secretShortcut, &QShortcut::activated, this, &MainWindow::openSettings);

    QShortcut *exitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(exitShortcut, &QShortcut::activated, qApp, &QApplication::quit);

    reloadCameras();
    configureScanner();

    this->showFullScreen();
    qDebug() << "System initialized.";
}

MainWindow::~MainWindow() {
    for(auto &cap : captures) if(cap.isOpened()) cap.release();
    if(serialScanner->isOpen()) serialScanner->close();
}

void MainWindow::ensureTmpFolderExists() {
    QDir dir("tmp");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

void MainWindow::setupStyles() {
    this->setStyleSheet(R"(
        QWidget#centralOverlay {
            border-image: url(:/img/bg.png) 0 0 0 0 stretch stretch;
        }
        QWidget#mainPanel {
            background-color: white;
            border-radius: 0px;
        }
        QLabel#headerTitle {
            font-family: 'Roboto Condensed', sans-serif;
            font-size: 34px;
            font-weight: 700;
            color: #001122;
            letter-spacing: 1px;
        }
        QLabel#dateLabel {
            font-family: 'Roboto', sans-serif;
            font-size: 26px;
            font-weight: 600;
            color: #444;
            margin-right: 20px;
        }
        QLabel.cameraDisplay {
            background-color: #111;
            border: 1px solid #aaa;
            color: #888;
            font-weight: bold;
        }
    )");
}

QLabel* MainWindow::createCameraLabel(const QString &text) {
    QLabel *l = new QLabel(text);
    l->setProperty("class", "cameraDisplay");
    l->setAlignment(Qt::AlignCenter);
    l->setScaledContents(false);
    l->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    l->setMinimumHeight(200);
    return l;
}

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralOverlay");

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(50, 50, 50, 50);

    mainPanel = new QWidget(this);
    mainPanel->setObjectName("mainPanel");

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 10);
    mainPanel->setGraphicsEffect(shadow);

    QVBoxLayout *panelLayout = new QVBoxLayout(mainPanel);
    panelLayout->setContentsMargins(25, 25, 25, 25);
    panelLayout->setSpacing(15);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    logoLabel = new QLabel();
    QPixmap logoPix(":/img/logo.png");
    if(!logoPix.isNull()) logoLabel->setPixmap(logoPix.scaledToHeight(60, Qt::SmoothTransformation));
    else logoLabel->setText("LOGO");

    headerTitle = new QLabel("Zeskanuj kod palety", this);
    headerTitle->setObjectName("headerTitle");
    headerTitle->setAlignment(Qt::AlignCenter);

    dateLabel = new QLabel(this);
    dateLabel->setObjectName("dateLabel");
    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, [this](){
        dateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    });
    clockTimer->start(1000);

    headerLayout->addWidget(logoLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(dateLabel);

    cam1_TopLeft = createCameraLabel("Kamera 1");
    cam4_TopRight = createCameraLabel("Kamera 4");
    cam0_BotLeft = createCameraLabel("Kamera 0");
    cam2_BotMid = createCameraLabel("Kamera 2");
    cam3_BotRight = createCameraLabel("Kamera 3");

    camDisplays = { cam0_BotLeft, cam1_TopLeft, cam2_BotMid, cam3_BotRight, cam4_TopRight };

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(15);
    topRow->addWidget(cam1_TopLeft, 1);
    topRow->addWidget(cam4_TopRight, 1);

    QHBoxLayout *botRow = new QHBoxLayout();
    botRow->setSpacing(15);
    botRow->addWidget(cam0_BotLeft, 1);
    botRow->addWidget(cam2_BotMid, 1);
    botRow->addWidget(cam3_BotRight, 1);

    panelLayout->addLayout(headerLayout);
    panelLayout->addLayout(topRow, 1);
    panelLayout->addLayout(botRow, 1);

    mainLayout->addWidget(mainPanel);
    setCentralWidget(centralWidget);
}

// --- FUNKCJA RYSOWANIA STATUSU I ANIMACJI ---
void MainWindow::drawStatusOnImage(int camIndex, const QPixmap &basePix, QString text, QColor overlayColor, bool isAnimated) {
    if (camIndex < 0 || camIndex >= camDisplays.size()) return;
    if (basePix.isNull()) return;

    // Tworzymy kopię, żeby rysować po niej
    QPixmap tempPix = basePix;
    QPainter painter(&tempPix);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = tempPix.width();
    int h = tempPix.height();

    // 1. Nakładka (Półprzezroczysty pasek/tło na środku)
    // Jeśli animacja (wysyłanie) -> Ciemne tło na całym obrazku dla skupienia
    if (isAnimated) {
        painter.fillRect(tempPix.rect(), QColor(0, 0, 0, 100)); // Przyciemnienie całego
    }

    // 2. Rysowanie animacji (Spinner)
    if (isAnimated) {
        int size = qMin(w, h) / 4; // Rozmiar kółka
        int cx = w / 2;
        int cy = h / 2;

        painter.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap));
        painter.translate(cx, cy);
        painter.rotate(spinnerAngle);
        // Rysujemy łuk (kółko z przerwą)
        painter.drawArc(-size/2, -size/2, size, size, 0, 270 * 16);
        painter.resetTransform();
    }

    // 3. Rysowanie Tekstu (Jeśli jest)
    if (!text.isEmpty()) {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(20);
        font.setBold(true);
        painter.setFont(font);

        // Jeśli to błąd, rysujemy czerwony pasek na górze
        if (!isAnimated) {
            QRect barRect(0, 0, w, 50);
            painter.fillRect(barRect, overlayColor); // Czerwony pasek
            painter.drawText(barRect, Qt::AlignCenter, text);
        } else {
            // Tekst pod spinnerem
            QRect textRect(0, (h/2) + (qMin(w,h)/8) + 10, w, 40);
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    }

    painter.end();

    // Skalowanie do labela
    QSize labelSize = camDisplays[camIndex]->size();
    if (!labelSize.isEmpty()) {
        camDisplays[camIndex]->setPixmap(tempPix.scaled(
            labelSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));
    }
}

// --- SLOT ANIMACJI (Wywoływany co 30ms) ---
void MainWindow::updateUploadAnimation() {
    spinnerAngle = (spinnerAngle + 15) % 360; // Obrót
    // Przerysowujemy nakładkę na bieżącej kamerze
    drawStatusOnImage(currentUploadCamIndex, currentBasePixmap, "WYSYŁANIE...", QColor(0,0,0,0), true);
}

void MainWindow::captureSnapshot() {
    qDebug() << "Snapshot for pallet:" << currentPalletCode;
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // TYLKO NUMER PALETY W NAGŁÓWKU
    headerTitle->setText("PALETA: " + currentPalletCode);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmm");

    for(size_t i=0; i<captures.size(); i++) {
        if(i >= camDisplays.size()) break;

        if(!captures[i].isOpened()) {
             QString url = settingsDialog->getCameraUrl(i);
             if(!url.isEmpty() && url != "0") captures[i].open(url.toStdString());
        }

        if(captures[i].isOpened()) {
            cv::Mat frame;
            bool success = false;

            for(int k=0; k<5; k++) {
                if(captures[i].read(frame) && !frame.empty()) success = true;
            }

            if(success && !frame.empty()) {
                cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
                QImage img((const uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);

                QString filename = QString("%1_%2_%3.jpg")
                        .arg(currentPalletCode)
                        .arg(timestamp)
                        .arg(i);

                QString fullPath = QDir("tmp").filePath(filename);

                if(img.save(fullPath, "JPG", 90)) {
                    // Wyświetl czyste zdjęcie
                    QSize labelSize = camDisplays[i]->size();
                    if(!labelSize.isEmpty()) {
                        QPixmap pix = QPixmap::fromImage(img);
                        camDisplays[i]->setPixmap(pix.scaled(
                            labelSize,
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation
                        ));
                    }

                    UploadJob job;
                    job.filePath = fullPath;
                    job.palletCode = currentPalletCode;
                    job.camIndex = (int)i;
                    uploadQueue.enqueue(job);
                }
            } else {
                camDisplays[i]->setText("Błąd obrazu");
            }
        } else {
             camDisplays[i]->setText("Brak połączenia");
        }
        QCoreApplication::processEvents();
    }

    QApplication::restoreOverrideCursor();

    if (!uploadQueue.isEmpty() && !isUploading) {
        startNextUpload();
    }
}

void MainWindow::startNextUpload() {
    if (uploadQueue.isEmpty()) {
        isUploading = false;
        return;
    }

    isUploading = true;
    UploadJob job = uploadQueue.head();

    currentUploadCamIndex = job.camIndex;
    currentBasePixmap.load(job.filePath);
    spinnerAngle = 0;
    animationTimer->start(33); // 30 FPS

    QUrl url("http://192.168.130.60:8000/php/upload.php");
    QUrlQuery query;
    query.addQueryItem("sulabel", job.palletCode);
    query.addQueryItem("cam", QString::number(job.camIndex));
    query.addQueryItem("gate", "2");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(5000);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    QString disposition = QString("form-data; name=\"photo\"; filename=\"%1\"")
                          .arg(QFileInfo(job.filePath).fileName());
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));

    QFile *file = new QFile(job.filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        animationTimer->stop();
        drawStatusOnImage(job.camIndex, currentBasePixmap, "BŁĄD PLIKU", Qt::red); // Alert
        delete multiPart;
        delete file;
        uploadQueue.dequeue();
        startNextUpload();
        return;
    }

    imagePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(imagePart);

    QNetworkReply *reply = netManager->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, [this, reply](){
        this->onUploadFinished(reply);
    });
}

void MainWindow::onUploadFinished(QNetworkReply *reply) {
    animationTimer->stop();

    if(uploadQueue.isEmpty()) { reply->deleteLater(); return; }
    UploadJob job = uploadQueue.head();

    if (reply->error() == QNetworkReply::NoError) {
        QSize labelSize = camDisplays[job.camIndex]->size();
        if(!labelSize.isEmpty() && !currentBasePixmap.isNull()) {
            camDisplays[job.camIndex]->setPixmap(currentBasePixmap.scaled(
                labelSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            ));
        }

        if(!uploadQueue.isEmpty()) uploadQueue.dequeue();

    } else {
        qDebug() << "Upload error:" << reply->errorString();
        drawStatusOnImage(job.camIndex, currentBasePixmap, "BŁĄD WYSYŁANIA", Qt::red);

        if(!uploadQueue.isEmpty()) uploadQueue.dequeue();
    }

    reply->deleteLater();
    startNextUpload();
}

void MainWindow::handleSerialScan() {
    QByteArray data = serialScanner->readAll();
    serialBuffer.append(data);

    if(serialBuffer.contains('\r') || serialBuffer.contains('\n')) {
        QString code = QString::fromUtf8(serialBuffer).trimmed();
        serialBuffer.clear();

        if(!code.isEmpty()) {
            currentPalletCode = code;
            headerTitle->setText("PALETA: " + code);
            captureSnapshot();
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    static QString keyBuffer;

    if(settingsDialog->getSelectedScannerPort() == "KEYBOARD") {
        if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if(!keyBuffer.isEmpty()) {
                currentPalletCode = keyBuffer;
                headerTitle->setText("PALETA: " + keyBuffer);
                captureSnapshot();
                keyBuffer.clear();
            }
        } else {
            if(!event->text().isEmpty() && event->text().at(0).isPrint()) {
                keyBuffer.append(event->text());
            }
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::configureScanner() {
    if(serialScanner->isOpen()) serialScanner->close();
    QString portName = settingsDialog->getSelectedScannerPort();

    if(portName == "KEYBOARD") {
        headerTitle->setText("Wczytaj kod palety");
    } else {
        serialScanner->setPortName(portName);
        serialScanner->setBaudRate(QSerialPort::Baud9600);
        if(serialScanner->open(QIODevice::ReadOnly)) {
            connect(serialScanner, &QSerialPort::readyRead, this, &MainWindow::handleSerialScan);
            headerTitle->setText("Gotowy (" + portName + ")");
        } else {
            headerTitle->setText("Błąd: " + portName);
        }
    }
}

void MainWindow::openSettings() {
    bool wasFullScreen = this->isFullScreen();
    if(wasFullScreen) this->showNormal();
    if(settingsDialog->exec() == QDialog::Accepted) {
        reloadCameras();
        configureScanner();
    }
    if(wasFullScreen) this->showFullScreen();
}

void MainWindow::reloadCameras() {
    for(auto &cap : captures) if(cap.isOpened()) cap.release();
    captures.clear();
    for(int i=0; i<5; i++) {
        QString url = settingsDialog->getCameraUrl(i);
        cv::VideoCapture cap;
        if(!url.isEmpty()) {
            if(url == "0") cap.open(0);
            else {
                cap.open(url.toStdString());
                cap.set(cv::CAP_PROP_BUFFERSIZE, 0);
            }
        }
        captures.push_back(cap);
    }
}