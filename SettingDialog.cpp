#include "SettingDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QCoreApplication>
#include <QSerialPortInfo>

SettingDialog::SettingDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Admin Panel (Ctrl+5)");
    setMinimumSize(600, 500);
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

    QWidget *tabCameras = new QWidget();
    QFormLayout *camLayout = new QFormLayout(tabCameras);
    cameraInputs.clear();
    for(int i=0; i<5; i++) {
        QLineEdit *edit = new QLineEdit();
        edit->setPlaceholderText("rtsp://... or 0");
        QString savedUrl = settings->value(QString("camera_%1").arg(i), "").toString();
        edit->setText(savedUrl);
        cameraInputs.push_back(edit);
        camLayout->addRow(QString("Camera %1:").arg(i+1), edit);
    }
    tabs->addTab(tabCameras, "Cameras");

    QWidget *tabScanner = new QWidget();
    QVBoxLayout *scanVBox = new QVBoxLayout(tabScanner);
    scannerSelector = new QComboBox();

    QPushButton *btnRefresh = new QPushButton("Refresh Ports");
    connect(btnRefresh, &QPushButton::clicked, this, &SettingDialog::refreshPorts);

    scanVBox->addWidget(new QLabel("Scanner Source:"));
    scanVBox->addWidget(scannerSelector);
    scanVBox->addWidget(btnRefresh);
    scanVBox->addStretch();
    tabs->addTab(tabScanner, "Scanner");

    QWidget *tabSystem = new QWidget();
    QFormLayout *sysLayout = new QFormLayout(tabSystem);

    spinWidth = new QSpinBox();
    spinWidth->setRange(800, 7680);
    spinWidth->setValue(settings->value("app_width", 1920).toInt());

    spinHeight = new QSpinBox();
    spinHeight->setRange(600, 4320);
    spinHeight->setValue(settings->value("app_height", 1080).toInt());

    checkFullScreen = new QCheckBox("Fullscreen Mode (Kiosk)");
    checkFullScreen->setChecked(settings->value("fullscreen", true).toBool());

    editServerUrl = new QLineEdit();
    editServerUrl->setText(settings->value("server_url", "http://192.168.130.60:8000/php/upload.php").toString());

    spinTimeout = new QSpinBox();
    spinTimeout->setRange(1, 60);
    spinTimeout->setSuffix(" s");
    spinTimeout->setValue(settings->value("upload_timeout", 5).toInt());

    sysLayout->addRow("Width:", spinWidth);
    sysLayout->addRow("Height:", spinHeight);
    sysLayout->addRow("", checkFullScreen);
    sysLayout->addRow("Server URL:", editServerUrl);
    sysLayout->addRow("Upload Timeout:", spinTimeout);

    tabs->addTab(tabSystem, "System");

    QPushButton *btnSave = new QPushButton("SAVE & RESTART");
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
    scannerSelector->addItem("Keyboard Mode (HID)", "KEYBOARD");

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

    for(size_t i=0; i<cameraInputs.size(); i++) {
        settings->setValue(QString("camera_%1").arg(i), cameraInputs[i]->text());
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

QString SettingDialog::getCameraUrl(int index) {
    QSettings *s = getSettings(); QString v = s->value(QString("camera_%1").arg(index), "").toString(); delete s; return v;
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