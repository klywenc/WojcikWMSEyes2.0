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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabs = new QTabWidget();
    QSettings *settings = getSettings();

    // --- CAMERAS TAB ---
    QWidget *tabCameras = new QWidget();
    QVBoxLayout *camVBox = new QVBoxLayout(tabCameras);

    // 1. Sekcja: Szablon URL (Najważniejsza zmiana)
    QGroupBox *grpTemplate = new QGroupBox("Zaawansowana Konfiguracja Linku RTSP");
    QFormLayout *templateLayout = new QFormLayout(grpTemplate);

    editUrlTemplate = new QLineEdit();
    // Domyślny szablon dla Dahua/Hikvision (z Twojego skryptu)
    QString defaultTemplate = "rtsp://%1:%2@%3:554/cam/realmonitor?channel=1&subtype=0&proto=Onvif";
    editUrlTemplate->setText(settings->value("rtsp_template", defaultTemplate).toString());

    QLabel *infoLabel = new QLabel("Legenda: %1 = Użytkownik, %2 = Hasło, %3 = Adres IP");
    infoLabel->setStyleSheet("color: gray; font-style: italic; font-size: 11px;");

    templateLayout->addRow("Szablon URL:", editUrlTemplate);
    templateLayout->addRow("", infoLabel);
    camVBox->addWidget(grpTemplate);

    // 2. Sekcja: Dane Logowania
    QGroupBox *grpAuth = new QGroupBox("Globalne Dane Logowania");
    QFormLayout *authLayout = new QFormLayout(grpAuth);

    editGlobalUser = new QLineEdit();
    editGlobalUser->setText(settings->value("cam_user", "snapshot1").toString());

    editGlobalPass = new QLineEdit();
    editGlobalPass->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    editGlobalPass->setText(settings->value("cam_pass", "snapshot1").toString());

    authLayout->addRow("Użytkownik (%1):", editGlobalUser);
    authLayout->addRow("Hasło (%2):", editGlobalPass);
    camVBox->addWidget(grpAuth);

    // 3. Sekcja: Lista Kamer
    QGroupBox *grpList = new QGroupBox("Adresy IP (%3) i Obrót");
    QGridLayout *gridCam = new QGridLayout(grpList);

    cameraRows.clear();
    for(int i=0; i<5; i++) {
        gridCam->addWidget(new QLabel(QString("Kamera %1:").arg(i+1)), i, 0);

        QLineEdit *editIp = new QLineEdit();
        editIp->setPlaceholderText("192.168.160.xx");
        editIp->setText(settings->value(QString("camera_%1_ip").arg(i), "").toString());

        QComboBox *comboRot = new QComboBox();
        comboRot->addItem("0° (Brak)", 0);
        comboRot->addItem("90° Prawo (CW)", 90);
        comboRot->addItem("180°", 180);
        comboRot->addItem("90° Lewo (CCW)", 270);

        int savedRot = settings->value(QString("camera_%1_rot").arg(i), 0).toInt();
        int idx = comboRot->findData(savedRot);
        if(idx != -1) comboRot->setCurrentIndex(idx);

        gridCam->addWidget(editIp, i, 1);
        gridCam->addWidget(comboRot, i, 2);

        CameraRow row;
        row.ipEdit = editIp;
        row.rotationCombo = comboRot;
        cameraRows.push_back(row);
    }
    camVBox->addWidget(grpList);
    tabs->addTab(tabCameras, "Kamery CCTV");

    // --- SCANNER TAB ---
    QWidget *tabScanner = new QWidget();
    QVBoxLayout *scanVBox = new QVBoxLayout(tabScanner);
    scannerSelector = new QComboBox();

    QPushButton *btnRefresh = new QPushButton("Odśwież Porty");
    connect(btnRefresh, &QPushButton::clicked, this, &SettingDialog::refreshPorts);

    scanVBox->addWidget(new QLabel("Źródło Skanera:"));
    scanVBox->addWidget(scannerSelector);
    scanVBox->addWidget(btnRefresh);
    scanVBox->addStretch();
    tabs->addTab(tabScanner, "Skaner");

    // --- SYSTEM TAB ---
    QWidget *tabSystem = new QWidget();
    QFormLayout *sysLayout = new QFormLayout(tabSystem);

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

    QPushButton *btnSave = new QPushButton("ZAPISZ I ZRESETUJ");
    btnSave->setStyleSheet("background-color: #d32f2f; color: white; font-weight: bold; padding: 10px;");
    connect(btnSave, &QPushButton::clicked, this, &SettingDialog::saveSettings);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(btnSave);

    delete settings;
    refreshPorts();
}

void SettingDialog::refreshPorts() {
    QSettings *settings = getSettings();
    QString savedPort = settings->value("scanner_port", "KEYBOARD").toString();
    delete settings;

    scannerSelector->clear();
    scannerSelector->addItem("Klawiatura / HID", "KEYBOARD");

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString label = info.portName();
        if(!info.description().isEmpty()) label += " (" + info.description() + ")";
        scannerSelector->addItem(label, info.portName());
    }

    int idx = scannerSelector->findData(savedPort);
    if(idx != -1) scannerSelector->setCurrentIndex(idx);
}

void SettingDialog::saveSettings() {
    QSettings *settings = getSettings();

    // Zapis Szablonu i Danych
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

QString SettingDialog::getUrlTemplate() {
    QSettings *s = getSettings();
    // Domyślny fallback, gdyby config był pusty
    QString defaultTmpl = "rtsp://%1:%2@%3:554/cam/realmonitor?channel=1&subtype=0&proto=Onvif";
    QString v = s->value("rtsp_template", defaultTmpl).toString();
    delete s; return v;
}

QString SettingDialog::getCameraIp(int index) {
    QSettings *s = getSettings(); QString v = s->value(QString("camera_%1_ip").arg(index), "").toString(); delete s; return v;
}
int SettingDialog::getCameraRotation(int index) {
    QSettings *s = getSettings(); int v = s->value(QString("camera_%1_rot").arg(index), 0).toInt(); delete s; return v;
}
QString SettingDialog::getGlobalUser() {
    QSettings *s = getSettings(); QString v = s->value("cam_user", "snapshot1").toString(); delete s; return v;
}
QString SettingDialog::getGlobalPass() {
    QSettings *s = getSettings(); QString v = s->value("cam_pass", "snapshot1").toString(); delete s; return v;
}
QString SettingDialog::getSelectedScannerPort() {
    QSettings *s = getSettings(); QString v = s->value("scanner_port", "KEYBOARD").toString(); delete s; return v;
}
int SettingDialog::getAppWidth() {
    QSettings *s = getSettings(); int v = s->value("app_width", 1920).toInt(); delete s; return v;
}
int SettingDialog::getAppHeight() {
    QSettings *s = getSettings(); int v = s->value("app_height", 1080).toInt(); delete s; return v;
}
bool SettingDialog::isFullScreen() {
    QSettings *s = getSettings(); bool v = s->value("fullscreen", true).toBool(); delete s; return v;
}
QString SettingDialog::getServerUrl() {
    QSettings *s = getSettings(); QString v = s->value("server_url", "http://192.168.130.60:8000/php/upload.php").toString(); delete s; return v;
}
int SettingDialog::getUploadTimeout() {
    QSettings *s = getSettings(); int v = s->value("upload_timeout", 5).toInt(); delete s; return v;
}
