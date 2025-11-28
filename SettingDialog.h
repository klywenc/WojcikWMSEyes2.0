#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSettings>
#include <QSerialPortInfo>
#include <vector>

class SettingDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingDialog(QWidget *parent = nullptr);
    QString getCameraUrl(int index);


    QString getSelectedScannerPort();

public slots:
    void saveSettings();
    void refreshPorts();

private:
    void setupUi();
    QSettings settings;
    std::vector<QLineEdit*> cameraInputs;
    QComboBox *scannerSelector;
};

#endif