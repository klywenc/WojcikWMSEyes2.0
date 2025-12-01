#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSettings>
#include <QSpinBox>
#include <QCheckBox>
#include <vector>

class SettingDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingDialog(QWidget *parent = nullptr);

    QString getCameraUrl(int index);
    QString getSelectedScannerPort();

    int getAppWidth();
    int getAppHeight();
    bool isFullScreen();
    QString getServerUrl();
    int getUploadTimeout();

public slots:
    void saveSettings();
    void refreshPorts();

private:
    void setupUi();
    QSettings* getSettings();

    std::vector<QLineEdit*> cameraInputs;
    QComboBox *scannerSelector;

    QSpinBox *spinWidth;
    QSpinBox *spinHeight;
    QCheckBox *checkFullScreen;
    QLineEdit *editServerUrl;
    QSpinBox *spinTimeout;
};

#endif