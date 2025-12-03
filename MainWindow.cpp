#include "MainWindow.h"
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
#include <QEventLoop>
#include <utility>

CameraWorker::CameraWorker(int index, QString url, int protocolMode, int rotation,
                           QString user, QString pass, QString savePath, QObject *parent)
    : QObject(parent), m_index(index), m_url(std::move(std::move(url))), m_protocol(protocolMode),
      m_rotation(rotation), m_user(std::move(user)), m_pass(std::move(pass)), m_savePath(std::move(savePath))
{
    setAutoDelete(true);
}

void CameraWorker::run() {
    qDebug() << "CAM_WORKER" << m_index << ": Started. Protocol:" << (m_protocol == 0 ? "HTTP" : "RTSP");

    QImage capturedImg;
    bool success = false;
    QString errorMsg = "";

    if (m_protocol == 0) {
        QNetworkAccessManager netMan;
        QNetworkRequest request(m_url);

        QString concatenated = m_user + ":" + m_pass;
        QByteArray data = concatenated.toLocal8Bit().toBase64();
        request.setRawHeader("Authorization", "Basic " + data);
        request.setTransferTimeout(5000); // 5s timeout

        QEventLoop loop;
        QObject::connect(&netMan, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
        QNetworkReply *reply = netMan.get(request);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imgData = reply->readAll();
            if (capturedImg.loadFromData(imgData)) success = true;
            else errorMsg = "Bad Image Data";
        } else {
            errorMsg = "HTTP: " + reply->errorString();
        }
        reply->deleteLater();
    }
    else {
        cv::VideoCapture cap;
        cap.open(m_url.toStdString(), cv::CAP_FFMPEG);

        if (cap.isOpened()) {
            cv::Mat frame;
            for(int k=0; k<15; k++) {
                if(cap.read(frame) && !frame.empty()) {
                    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
                    capturedImg = QImage(static_cast<const uchar *>(frame.data), frame.cols, frame.rows, frame.step, QImage::Format_RGB888).copy();
                    success = true;
                    break;
                }
            }
            if(!success) errorMsg = "Decode Fail";
            cap.release();
        } else {
            errorMsg = "Connect Fail";
        }
    }

    if (success && !capturedImg.isNull()) {
        if (m_rotation != 0) {
            QTransform trans;
            trans.rotate(m_rotation == 270 ? -90 : m_rotation);
            capturedImg = capturedImg.transformed(trans);
        }

        if (!capturedImg.save(m_savePath, "JPG", 85)) {
            success = false;
            errorMsg = "Disk Write Error";
        }
    }

    emit resultReady(m_index, success, m_savePath, errorMsg);
    qDebug() << "CAM_WORKER" << m_index << ": Finished. Success:" << success;
}

UploadWorker::UploadWorker(QString serverUrl, int timeout, QObject *parent)
    : QObject(parent), m_serverUrl(std::move(serverUrl)), m_timeout(timeout), m_isUploading(false)
{
    manager = new QNetworkAccessManager(this);
}

void UploadWorker::addJob(const UploadJob& job) {
    m_queue.enqueue(job);
    processNext();
}

void UploadWorker::processNext() {
    if (m_isUploading || m_queue.isEmpty()) return;

    m_isUploading = true;
    UploadJob job = m_queue.head();
    sendRequest(job);
}

void UploadWorker::sendRequest(const UploadJob &job) {
    emit uploadStarted(job.camIndex);

    QUrl url(m_serverUrl);
    QUrlQuery query;
    query.addQueryItem("sulabel", job.palletCode);
    query.addQueryItem("cam", QString::number(job.camIndex));
    query.addQueryItem("gate", "2");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeout * 1000);

    // 2. Przygotowanie pliku i multipart
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"photo\"; filename=\"%1\"").arg(QFileInfo(job.filePath).fileName())));

    auto *file = new QFile(job.filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qCritical() << "UPLOAD_WORKER: File open error:" << job.filePath;
        delete multiPart; delete file;
        m_isUploading = false;
        m_queue.dequeue();
        emit uploadFinished(job.camIndex, false, "File Error");
        processNext();
        return;
    }

    qint64 fileSize = file->size();
    imagePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(imagePart);

    // --- LOGOWANIE WYSYŁANYCH DANYCH (REQUEST) ---
    qDebug() << "\n>>> [HTTP REQUEST OUT] >>>";
    qDebug() << "Method: POST";
    qDebug() << "URL:" << url.toString();
    qDebug() << "Payload: Multipart Form-Data";
    qDebug() << "File:" << job.filePath << "| Size:" << fileSize << "bytes";
    // Logujemy nagłówki, które ustawiliśmy ręcznie (reszta jest dodawana automatycznie przez QNAM)
    qDebug() << "Known Headers:" << request.rawHeaderList();
    qDebug() << ">>> ------------------------ >>>";

    // 3. Wysłanie żądania
    QEventLoop loop;
    QObject::connect(manager, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
    QNetworkReply *reply = manager->post(request, multiPart);
    multiPart->setParent(reply);

    loop.exec();

    bool success = (reply->error() == QNetworkReply::NoError);
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseBody = reply->readAll();
    QString msg = success ? "OK" : reply->errorString();

    qDebug() << "\n<<< [HTTP RESPONSE IN] <<<";
    qDebug() << "Status Code:" << statusCode;

    // Logowanie wszystkich nagłówków otrzymanych od serwera PHP/Apache/Nginx
    qDebug() << "Headers:";
    const auto headerList = reply->rawHeaderList();
    for (const auto &head : headerList) {
        qDebug() << " -" << head << ":" << reply->rawHeader(head);
    }

    qDebug() << "BODY (Payload):" << responseBody;
    qDebug() << "<<< ---------------------- <<<";

    reply->deleteLater();

    m_queue.dequeue();
    m_isUploading = false;
    emit uploadFinished(job.camIndex, success, msg);
    processNext();
}
// gui thread

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    this->setObjectName("mainWindow");
    qDebug() << "APP: Starting...";

    // Wymuszenie tcp aby sie nie pierdolilo
    qputenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp");
    // wywalenie gstreamera bo linux tego nie obsluguje, ale na windzie to normalnie dziala
    qputenv("OPENCV_VIDEOIO_PRIORITY_GSTREAMER", "0");

    settingsDialog = new SettingDialog(this);

    const int w = settingsDialog->getAppWidth();
    const int h = settingsDialog->getAppHeight();
    const bool fs = settingsDialog->isFullScreen();

    if (fs) {
        this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        this->showFullScreen();
    } else {
        this->setWindowFlags(Qt::Window);
        this->resize(w, h);
        this->show();
    }

    // port seriala do scanera
    serialScanner = new QSerialPort(this);

    // pool do kamerek
    cameraPool = QThreadPool::globalInstance();
    cameraPool->setMaxThreadCount(8);

    uploadThread = new QThread(this);
    uploadWorker = new UploadWorker(settingsDialog->getServerUrl(), settingsDialog->getUploadTimeout());
    uploadWorker->moveToThread(uploadThread);

    connect(uploadThread, &QThread::finished, uploadWorker, &QObject::deleteLater);
    connect(this, &MainWindow::requestUpload, uploadWorker, &UploadWorker::addJob);
    connect(uploadWorker, &UploadWorker::uploadStarted, this, &MainWindow::onWorkerUploadStarted);
    connect(uploadWorker, &UploadWorker::uploadFinished, this, &MainWindow::onWorkerUploadFinished);

    uploadThread->start();

    ensureTmpFolderExists();
    setupStyles();
    setupUi();

    secretShortcut = new QShortcut(QKeySequence("Ctrl+5"), this);
    connect(secretShortcut, &QShortcut::activated, this, &MainWindow::openSettings);

    auto *exitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(exitShortcut, &QShortcut::activated, qApp, &QApplication::quit);

    configureScanner();
}

MainWindow::~MainWindow() {
    qDebug() << "APP: Closing...";
    if(uploadThread->isRunning()) {
        uploadThread->quit();
        uploadThread->wait();
    }
    if(serialScanner->isOpen()) serialScanner->close();
}

void MainWindow::ensureTmpFolderExists() {
    QDir dir("tmp");
    if (!dir.exists()) dir.mkpath(".");
}

void MainWindow::setupStyles() {
    this->setStyleSheet(R"(
        QWidget#centralOverlay { border-image: url(:/img/bg.png) 0 0 0 0 stretch stretch; }
        QWidget#mainPanel { background-color: white; border-radius: 0px; }
        QLabel#headerTitle { font-family: 'Roboto Condensed'; font-size: 34px; font-weight: 700; color: #001122; letter-spacing: 1px; }
        QLabel#dateLabel { font-family: 'Roboto'; font-size: 26px; font-weight: 600; color: #444; margin-right: 20px; }
        QLabel.cameraDisplay { background-color: #222; border: 2px solid #ccc; color: #888; font-weight: bold; }
    )");
}

QLabel* MainWindow::createCameraLabel(const QString &text) {
    auto *l = new QLabel(text);
    l->setProperty("class", "cameraDisplay");
    l->setAlignment(Qt::AlignCenter);
    l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    l->setScaledContents(false);
    return l;
}

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralOverlay");
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(40, 40, 40, 40);

    mainPanel = new QWidget(this);
    mainPanel->setObjectName("mainPanel");
    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(40); shadow->setColor(QColor(0, 0, 0, 120)); shadow->setOffset(0, 10);
    mainPanel->setGraphicsEffect(shadow);

    auto *panelLayout = new QVBoxLayout(mainPanel);
    panelLayout->setContentsMargins(20, 20, 20, 20); panelLayout->setSpacing(15);

    auto *headerLayout = new QHBoxLayout();
    logoLabel = new QLabel();
    QPixmap logoPix(":/img/logo.png");
    if(!logoPix.isNull()) logoLabel->setPixmap(logoPix.scaledToHeight(60, Qt::SmoothTransformation));
    else logoLabel->setText("LOGO");

    headerTitle = new QLabel("ZESKANUJ KOD PALETY", this);
    headerTitle->setObjectName("headerTitle");
    headerTitle->setAlignment(Qt::AlignCenter);

    dateLabel = new QLabel(this);
    dateLabel->setObjectName("dateLabel");
    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer->start(1000);

    headerLayout->addWidget(logoLabel); headerLayout->addStretch();
    headerLayout->addWidget(headerTitle); headerLayout->addStretch();
    headerLayout->addWidget(dateLabel);

    cam1_TopLeft = createCameraLabel("Kamera 1"); cam4_TopRight = createCameraLabel("Kamera 4");
    cam0_BotLeft = createCameraLabel("Kamera 0"); cam2_BotMid = createCameraLabel("Kamera 2"); cam3_BotRight = createCameraLabel("Kamera 3");
    camDisplays = { cam0_BotLeft, cam1_TopLeft, cam2_BotMid, cam3_BotRight, cam4_TopRight };

    auto *topRow = new QHBoxLayout(); topRow->setSpacing(15);
    topRow->addWidget(cam1_TopLeft, 1); topRow->addWidget(cam4_TopRight, 1);
    auto *botRow = new QHBoxLayout(); botRow->setSpacing(15);
    botRow->addWidget(cam0_BotLeft, 1); botRow->addWidget(cam2_BotMid, 1); botRow->addWidget(cam3_BotRight, 1);

    panelLayout->addLayout(headerLayout); panelLayout->addLayout(topRow, 1); panelLayout->addLayout(botRow, 1);
    mainLayout->addWidget(mainPanel); setCentralWidget(centralWidget);
}

void MainWindow::updateClock() const {
    dateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::drawStatusOnImage(int camIndex, const QString &filePath, int status, const QString &msg) {
    // Statusy 0 - brak, 1 - sending, 2 - OK, 3 - error
    if (camIndex < 0 || camIndex >= camDisplays.size()) return;
    QPixmap pix(filePath);
    if (pix.isNull()) return;

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    if (status > 0) {
        QColor color;
        QString text = msg;
        if (status == 1) { color = QColor(0, 120, 215); if(text.isEmpty()) text="WYSYŁANIE..."; }
        else if (status == 2) { color = QColor(40, 167, 69); if(text.isEmpty()) text="WYSŁANO \u2714"; } // ✔
        else { color = QColor(220, 53, 69); if(text.isEmpty()) text="BŁĄD \u274C"; } // ❌

        int w = pix.width();
        int h = pix.height();
        int barHeight = qMax(40, h/12);

        painter.fillRect(QRect(0, 0, w, barHeight), color);

        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(barHeight * 0.6);
        font.setBold(true);
        painter.setFont(font);

        painter.drawText(QRect(0, 0, w, barHeight), Qt::AlignCenter, text);
    }
    painter.end();

    QLabel *lbl = camDisplays[camIndex];
    if (!lbl->size().isEmpty()) {
        lbl->setPixmap(pix.scaled(lbl->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MainWindow::startScanProcess() {
    qDebug() << "SCAN: New Code -> " << currentPalletCode;
    headerTitle->setText("PALETA: " + currentPalletCode);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmm");
    QString user = settingsDialog->getGlobalUser();
    QString pass = settingsDialog->getGlobalPass();
    QString urlTemplate = settingsDialog->getUrlTemplate();
    int mode = settingsDialog->getProtocolMode();

    for(int i=0; i<5; i++) {
        QString ip = settingsDialog->getCameraIp(i);
        if (ip.trimmed().isEmpty() || ip == "0") continue;

        QString url = urlTemplate;
        // szablony do urli
        url.replace("%1", user);
        url.replace("%2", pass);
        url.replace("%3", ip);

        QString filename = QString("%1_%2_%3.jpg").arg(currentPalletCode, timestamp).arg(i);
        const QString savePath = QDir("tmp").filePath(filename);
        const int rotation = settingsDialog->getCameraRotation(i);

        camDisplays[i]->setText("POBIERANIE...");

        auto *worker = new CameraWorker(i, url, mode, rotation, user, pass, savePath);
        connect(worker, &CameraWorker::resultReady, this, &MainWindow::onCameraFinished);

        cameraPool->start(worker);
    }
}

void MainWindow::onCameraFinished(int index, bool success, const QString& filePath, const QString& errorMsg) {
    if (success) {
        lastImagePaths[index] = filePath;
        drawStatusOnImage(index, filePath, 0);

        UploadJob job;
        job.filePath = filePath;
        job.palletCode = currentPalletCode;
        job.camIndex = index;

        emit requestUpload(job);

    } else {
        qWarning() << "MAIN: Cam" << index << "Failed:" << errorMsg;
        camDisplays[index]->setText("BŁĄD:\n" + errorMsg);
    }
}

void MainWindow::onWorkerUploadStarted(const int camIndex) {
    if (!lastImagePaths[camIndex].isEmpty()) {
        drawStatusOnImage(camIndex, lastImagePaths[camIndex], 1);
    }
}

void MainWindow::onWorkerUploadFinished(const int camIndex, const bool success, const QString& message) {
    if (!lastImagePaths[camIndex].isEmpty()) {
        drawStatusOnImage(camIndex, lastImagePaths[camIndex], success ? 2 : 3);
    }
}

void MainWindow::handleSerialScan() {
    const QByteArray data = serialScanner->readAll();
    serialBuffer.append(data);
    if(serialBuffer.contains('\r') || serialBuffer.contains('\n')) {
        const QString code = QString::fromUtf8(serialBuffer).trimmed();
        serialBuffer.clear();
        if(!code.isEmpty()) {
            qDebug() << "SERIAL: Input ->" << code;
            currentPalletCode = code;
            startScanProcess();
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    static QString keyBuffer;
    if(settingsDialog->getSelectedScannerPort() == "KEYBOARD") {
        if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if(!keyBuffer.isEmpty()) {
                qDebug() << "KBD: Input ->" << keyBuffer;
                currentPalletCode = keyBuffer;
                startScanProcess();
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

    qDebug() << "SCANNER: Configuring..." << portName;

    if(portName == "KEYBOARD") headerTitle->setText("ZESKANUJ KOD PALETY");
    else {
        serialScanner->setPortName(portName);
        serialScanner->setBaudRate(QSerialPort::Baud9600);
        if(serialScanner->open(QIODevice::ReadOnly)) {
            connect(serialScanner, &QSerialPort::readyRead, this, &MainWindow::handleSerialScan);
            headerTitle->setText("GOTOWY (" + portName + ")");
            qDebug() << "SCANNER: Serial Port Open";
        } else {
            headerTitle->setText("BŁĄD SKANERA");
            qCritical() << "SCANNER: Failed to open port";
        }
    }
}

void MainWindow::openSettings() {
    bool wasFullScreen = this->isFullScreen();
    if(wasFullScreen) this->showNormal();

    if(settingsDialog->exec() == QDialog::Accepted) {
        qDebug() << "SETTINGS: Saved. Restarting subsystems...";

        disconnect(uploadThread, &QThread::finished, uploadWorker, &QObject::deleteLater);

        if(uploadThread->isRunning()) {
            uploadThread->quit();
            uploadThread->wait();
        }

        delete uploadWorker;

        uploadWorker = new UploadWorker(settingsDialog->getServerUrl(), settingsDialog->getUploadTimeout());
        uploadWorker->moveToThread(uploadThread);

        connect(uploadThread, &QThread::finished, uploadWorker, &QObject::deleteLater);
        connect(this, &MainWindow::requestUpload, uploadWorker, &UploadWorker::addJob);
        connect(uploadWorker, &UploadWorker::uploadStarted, this, &MainWindow::onWorkerUploadStarted);
        connect(uploadWorker, &UploadWorker::uploadFinished, this, &MainWindow::onWorkerUploadFinished);

        uploadThread->start();

        configureScanner();
    }

    if(wasFullScreen) this->showFullScreen();
}
