#include "SettingDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QGroupBox>

SettingDialog::SettingDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Panel Administratora (Ctrl+5)");
    setMinimumSize(750, 650);
    setupUi();
}

QSettings* SettingDialog::getSettings() {
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    return new QSettings(configPath, QSettings::IniFormat);
}

void SettingDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget();
    QSettings *settings = getSettings();

    auto *tabCameras = new QWidget();
    auto *camVBox = new QVBoxLayout(tabCameras);

    auto *grpTemplate = new QGroupBox("Konfiguracja Protokołu i Linku");
    auto *templateLayout = new QFormLayout(grpTemplate);

    comboProtocol = new QComboBox();
    comboProtocol->addItem("HTTP (Zdjęcie / Wget) - Zalecane", 0);
    comboProtocol->addItem("RTSP (Strumień / FFmpeg)", 1);

    int savedProto = settings->value("protocol_index", 0).toInt();
    comboProtocol->setCurrentIndex(savedProto);
    connect(comboProtocol, SIGNAL(currentIndexChanged(int)), this, SLOT(onProtocolChanged(int)));

    editUrlTemplate = new QLineEdit();
    QString defaultTmpl = "http://%3/cgi-bin/snapshot.cgi?channel=1";
    editUrlTemplate->setText(settings->value("rtsp_template", defaultTmpl).toString());

    auto *helpLabel = new QLabel(
        "<b>Legenda:</b> "
        "<span style='color: #d32f2f;'><b>%1</b></span>=Użytkownik, "
        "<span style='color: #d32f2f;'><b>%2</b></span>=Hasło, "
        "<span style='color: #d32f2f;'><b>%3</b></span>=IP<br>"
        "Dla HTTP User/Pass są wysyłane w nagłówku (niewidoczne w URL)."
    );
    helpLabel->setTextFormat(Qt::RichText);
    helpLabel->setStyleSheet("color: #555; font-size: 11px; margin-top: 5px;");

    templateLayout->addRow("Protokół:", comboProtocol);
    templateLayout->addRow("Szablon URL:", editUrlTemplate);
    templateLayout->addWidget(helpLabel);
    camVBox->addWidget(grpTemplate);

    auto *grpAuth = new QGroupBox("Globalne Dane Logowania");
    auto *authLayout = new QFormLayout(grpAuth);

    editGlobalUser = new QLineEdit();
    editGlobalUser->setText(settings->value("cam_user", "snapshot1").toString());

    editGlobalPass = new QLineEdit();
    editGlobalPass->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    editGlobalPass->setText(settings->value("cam_pass", "snapshot1").toString());

    authLayout->addRow("Użytkownik (%1):", editGlobalUser);
    authLayout->addRow("Hasło (%2):", editGlobalPass);
    camVBox->addWidget(grpAuth);

    auto *grpList = new QGroupBox("Adresy IP (%3) i Obrót");
    auto *gridCam = new QGridLayout(grpList);

    cameraRows.clear();
    for(int i=0; i<5; i++) {
        gridCam->addWidget(new QLabel(QString("Kamera %1:").arg(i+1)), i, 0);

        auto *editIp = new QLineEdit();
        editIp->setPlaceholderText("192.168.160.xx");
        editIp->setText(settings->value(QString("camera_%1_ip").arg(i), "").toString());

        auto *comboRot = new QComboBox();
        comboRot->addItem("0° (Brak)", 0);
        comboRot->addItem("90° Prawo (CW)", 90);
        comboRot->addItem("180°", 180);
        comboRot->addItem("90° Lewo (CCW)", 270);

        int savedRot = settings->value(QString("camera_%1_rot").arg(i), 0).toInt();
        int idx = comboRot->findData(savedRot);
        if(idx != -1) comboRot->setCurrentIndex(idx);

        gridCam->addWidget(editIp, i, 1);
        gridCam->addWidget(comboRot, i, 2);

        CameraRow row{};
        row.ipEdit = editIp;
        row.rotationCombo = comboRot;
        cameraRows.push_back(row);
    }
    camVBox->addWidget(grpList);
    tabs->addTab(tabCameras, "Kamery CCTV");

    auto *tabScanner = new QWidget();
    auto *scanVBox = new QVBoxLayout(tabScanner);
    scannerSelector = new QComboBox();

    auto *btnRefresh = new QPushButton("Odśwież Porty");
    connect(btnRefresh, &QPushButton::clicked, this, &SettingDialog::refreshPorts);

    scanVBox->addWidget(new QLabel("Źródło Skanera:"));
    scanVBox->addWidget(scannerSelector);
    scanVBox->addWidget(btnRefresh);
    scanVBox->addStretch();
    tabs->addTab(tabScanner, "Skaner");

    auto *tabSystem = new QWidget();
    auto *sysLayout = new QFormLayout(tabSystem);

    spinWidth = new QSpinBox();
    spinWidth->setRange(800, 7680);
    spinWidth->setValue(settings->value("app_width", 1920).toInt());

    spinHeight = new QSpinBox();
    spinHeight->setRange(600, 4320);
    spinHeight->setValue(settings->value("app_height", 1080).toInt());

    checkFullScreen = new QCheckBox("Tryb Pełnoekranowy (Kiosk)");
    checkFullScreen->setChecked(settings->value("fullscreen", true).toBool());

    editServerUrl = new QLineEdit();
    editServerUrl->setText(settings->value("server_url", "http://192.168.130.60:8000/php/upload.php").toString());

    spinTimeout = new QSpinBox();
    spinTimeout->setRange(1, 60);
    spinTimeout->setSuffix(" s");
    spinTimeout->setValue(settings->value("upload_timeout", 5).toInt());

    sysLayout->addRow("Szerokość:", spinWidth);
    sysLayout->addRow("Wysokość:", spinHeight);
    sysLayout->addRow("", checkFullScreen);
    sysLayout->addRow("Adres URL Servera:", editServerUrl);
    sysLayout->addRow("Limit czasu (Timeout):", spinTimeout);

    tabs->addTab(tabSystem, "System");

    auto *btnSave = new QPushButton("ZAPISZ I ZRESETUJ");
    btnSave->setStyleSheet("background-color: #d32f2f; color: white; font-weight: bold; padding: 10px;");
    connect(btnSave, &QPushButton::clicked, this, &SettingDialog::saveSettings);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(btnSave);

    delete settings;
    refreshPorts();
}

void SettingDialog::onProtocolChanged(int index) const {
    if (index == 0) {
        editUrlTemplate->setText("http://%3/cgi-bin/snapshot.cgi?channel=1");
    } else {
        // editUrlTemplate->setText("rtsp://%3/h264.sdp");
        editUrlTemplate->setText("rtsp://%1:%2@%3:554/cam/realmonitor?channel=1&subtype=0&proto=Onvif");
    }
}

void SettingDialog::refreshPorts() const {
    const QSettings *settings = getSettings();
    const QString savedPort = settings->value("scanner_port", "KEYBOARD").toString();
    delete settings;

    scannerSelector->clear();
    scannerSelector->addItem("Klawiatura / HID", "KEYBOARD");

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString label = info.portName();
        if(!info.description().isEmpty()) label += " (" + info.description() + ")";
        scannerSelector->addItem(label, info.portName());
    }

    if(const int idx = scannerSelector->findData(savedPort); idx != -1) scannerSelector->setCurrentIndex(idx);
}

void SettingDialog::saveSettings() {
    QSettings *settings = getSettings();

    settings->setValue("protocol_index", comboProtocol->currentIndex());
    settings->setValue("rtsp_template", editUrlTemplate->text());
    settings->setValue("cam_user", editGlobalUser->text());
    settings->setValue("cam_pass", editGlobalPass->text());

    for(size_t i=0; i<cameraRows.size(); i++) {
        settings->setValue(QString("camera_%1_ip").arg(i), cameraRows[i].ipEdit->text());
        settings->setValue(QString("camera_%1_rot").arg(i), cameraRows[i].rotationCombo->currentData());
    }

    settings->setValue("scanner_port", scannerSelector->currentData().toString());
    settings->setValue("app_width", spinWidth->value());
    settings->setValue("app_height", spinHeight->value());
    settings->setValue("fullscreen", checkFullScreen->isChecked());
    settings->setValue("server_url", editServerUrl->text());
    settings->setValue("upload_timeout", spinTimeout->value());

    delete settings;
    accept();
}

int SettingDialog::getProtocolMode() {
    QSettings *s = getSettings(); int v = s->value("protocol_index", 0).toInt(); delete s; return v;
}
QString SettingDialog::getUrlTemplate() {
    const QSettings *s = getSettings();
    const QString defaultTmpl = "http://%3/cgi-bin/snapshot.cgi?channel=1";
    QString v = s->value("rtsp_template", defaultTmpl).toString();
    delete s; return v;
}
QString SettingDialog::getCameraIp(int index) {
    const QSettings *s = getSettings(); QString v = s->value(QString("camera_%1_ip").arg(index), "").toString(); delete s; return v;
}
int SettingDialog::getCameraRotation(int index) {
    const QSettings *s = getSettings(); int v = s->value(QString("camera_%1_rot").arg(index), 0).toInt(); delete s; return v;
}
QString SettingDialog::getGlobalUser() {
    const QSettings *s = getSettings(); QString v = s->value("cam_user", "snapshot1").toString(); delete s; return v;
}
QString SettingDialog::getGlobalPass() {
    const QSettings *s = getSettings(); QString v = s->value("cam_pass", "snapshot1").toString(); delete s; return v;
}
QString SettingDialog::getSelectedScannerPort() {
    const QSettings *s = getSettings(); QString v = s->value("scanner_port", "KEYBOARD").toString(); delete s; return v;
}
int SettingDialog::getAppWidth() {
    const QSettings *s = getSettings(); int v = s->value("app_width", 1920).toInt(); delete s; return v;
}
int SettingDialog::getAppHeight() {
    const QSettings *s = getSettings(); int v = s->value("app_height", 1080).toInt(); delete s; return v;
}
bool SettingDialog::isFullScreen() {
    const QSettings *s = getSettings(); bool v = s->value("fullscreen", true).toBool(); delete s; return v;
}
QString SettingDialog::getServerUrl() {
    const QSettings *s = getSettings(); QString v = s->value("server_url", "http://192.168.130.60:8000/php/upload.php").toString(); delete s; return v;
}
int SettingDialog::getUploadTimeout() {
    const QSettings *s = getSettings(); int v = s->value("upload_timeout", 5).toInt(); delete s; return v;
}
