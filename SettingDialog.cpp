#include "SettingDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

SettingDialog::SettingDialog(QWidget *parent)
    : QDialog(parent), settings("MebleWojcik", "MagazynApp")
{
    setWindowTitle("Konfiguracja Systemu (Ctrl+5)");
    setMinimumSize(500, 500);
    setupUi();
}

void SettingDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- SEKCJA KAMER ---
    QGroupBox *grpCameras = new QGroupBox("Kamery CCTV");
    QFormLayout *camLayout = new QFormLayout(grpCameras);
    cameraInputs.clear();
    for(int i=0; i<5; i++) {
        QLineEdit *edit = new QLineEdit();
        edit->setPlaceholderText("rtsp://... lub 0 dla USB");
        QString savedUrl = settings.value(QString("camera_%1").arg(i), "").toString();
        edit->setText(savedUrl);
        cameraInputs.push_back(edit);
        camLayout->addRow(QString("Kamera %1:").arg(i), edit);
    }
    mainLayout->addWidget(grpCameras);

    // --- SEKCJA SKANERA (Nowość) ---
    QGroupBox *grpScanner = new QGroupBox("Skaner Kodów (USB / Bluetooth)");
    QVBoxLayout *scanVBox = new QVBoxLayout(grpScanner);

    QLabel *info = new QLabel("Wybierz 'Klawiatura' (domyślne) lub port COM skanera.");
    info->setStyleSheet("color: gray; font-style: italic; font-size: 11px;");

    QHBoxLayout *scanRow = new QHBoxLayout();
    scannerSelector = new QComboBox();
    QPushButton *btnRefresh = new QPushButton("Odśwież listę");
    connect(btnRefresh, &QPushButton::clicked, this, &SettingDialog::refreshPorts);

    scanRow->addWidget(scannerSelector, 1);
    scanRow->addWidget(btnRefresh);

    scanVBox->addWidget(info);
    scanVBox->addLayout(scanRow);
    mainLayout->addWidget(grpScanner);

    // --- PRZYCISKI ---
    QPushButton *btnSave = new QPushButton("Zapisz i Zamknij");
    btnSave->setStyleSheet("background-color: #d32f2f; color: white; padding: 10px; font-weight: bold;");
    connect(btnSave, &QPushButton::clicked, this, &SettingDialog::saveSettings);
    mainLayout->addWidget(btnSave);

    // Załaduj listę przy starcie
    refreshPorts();
}

void SettingDialog::refreshPorts() {
    scannerSelector->clear();

    // Opcja 1: Tryb klawiatury (Standardowy skaner USB HID)
    scannerSelector->addItem("Tryb Klawiatury (HID)", "KEYBOARD");

    // Opcja 2: Skanowanie portów (Bluetooth SPP / USB VCP)
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString label = info.portName();
        if(!info.description().isEmpty())
            label += " (" + info.description() + ")";

        scannerSelector->addItem(label, info.portName());
    }

    // Ustawienie zapamiętanej opcji
    QString savedPort = settings.value("scanner_port", "KEYBOARD").toString();
    int idx = scannerSelector->findData(savedPort);
    if(idx != -1) scannerSelector->setCurrentIndex(idx);
}

void SettingDialog::saveSettings() {
    for(size_t i=0; i<cameraInputs.size(); i++) {
        settings.setValue(QString("camera_%1").arg(i), cameraInputs[i]->text());
    }
    settings.setValue("scanner_port", scannerSelector->currentData().toString());
    accept();
}

QString SettingDialog::getCameraUrl(int index) {
    return settings.value(QString("camera_%1").arg(index), "").toString();
}

QString SettingDialog::getSelectedScannerPort() {
    return settings.value("scanner_port", "KEYBOARD").toString();
}