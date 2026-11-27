/**
 * SettingsDialog - Application-wide settings
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

private slots:
    void accept() override;
    void browseRecordDir();

private:
    void buildUi();
    void loadSettings();
    void saveSettings();

    // Global settings tab
    QComboBox *m_traceLevelCombo = nullptr;
    QComboBox *m_nmeaReqCombo = nullptr;
    QSpinBox *m_fswapMarginSpin = nullptr;
    QSpinBox *m_relayBackSpin = nullptr;
    QSpinBox *m_staIdSpin = nullptr;
    QSpinBox *m_staSelSpin = nullptr;
    QLineEdit *m_antTypeEdit = nullptr;
    QLineEdit *m_rcvTypeEdit = nullptr;

    // Path settings tab
    QLineEdit *m_logFileEdit = nullptr;
    QLineEdit *m_srctblFileEdit = nullptr;
    QLineEdit *m_staposFileEdit = nullptr;
    QLineEdit *m_proxyAddrEdit = nullptr;

    // Server options
    QSpinBox *m_srvOpt[6] = {nullptr};

    // UI language
    QComboBox *m_langCombo = nullptr;

    // RTCM record
    QCheckBox *m_recordEnableCheck = nullptr;
    QLineEdit *m_recordDirEdit = nullptr;
    QLabel *m_recordFileLabel = nullptr;
};

#endif // SETTINGSDIALOG_H
