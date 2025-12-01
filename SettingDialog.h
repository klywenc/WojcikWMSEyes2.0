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

    QString getCameraIp(int index);
    int getCameraRotation(int index);
    QString getGlobalUser();
    QString getGlobalPass();
    QString getUrlTemplate();
    int getProtocolMode(); // 0 = HTTP, 1 = RTSP

    QString getSelectedScannerPort();
    int getAppWidth();
    int getAppHeight();
    bool isFullScreen();
    QString getServerUrl();
    int getUploadTimeout();

public slots:
    void saveSettings();
    void refreshPorts();
    void onProtocolChanged(int index);

private:
    void setupUi();
    QSettings* getSettings();

    struct CameraRow {
        QLineEdit *ipEdit;
        QComboBox *rotationCombo;
    };

    QLineEdit *editGlobalUser;
    QLineEdit *editGlobalPass;
    QComboBox *comboProtocol;
    QLineEdit *editUrlTemplate;

    std::vector<CameraRow> cameraRows;
    QComboBox *scannerSelector;
    QSpinBox *spinWidth;
    QSpinBox *spinHeight;
    QCheckBox *checkFullScreen;
    QLineEdit *editServerUrl;
    QSpinBox *spinTimeout;
};

#endif
