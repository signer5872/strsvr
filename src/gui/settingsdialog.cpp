#include "settingsdialog.h"
#include "../core/appconfig.h"
#include "../core/logger.h"
#include "../core/language.h"
#include "../core/rtcmrecorder.h"

#include <QPushButton>
#include <QFileDialog>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    resize(500, 400);

    buildUi();
    loadSettings();
}

void SettingsDialog::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QTabWidget *tabWidget = new QTabWidget(this);

    // Global Settings Tab
    QWidget *globalTab = new QWidget();
    QFormLayout *globalLayout = new QFormLayout(globalTab);

    m_traceLevelCombo = new QComboBox();
    m_traceLevelCombo->addItem(tr("0 - Verbose"), 0);
    m_traceLevelCombo->addItem(tr("1 - Debug"), 1);
    m_traceLevelCombo->addItem(tr("2 - Info"), 2);
    m_traceLevelCombo->addItem(tr("3 - Warning"), 3);
    m_traceLevelCombo->addItem(tr("4 - Error"), 4);
    globalLayout->addRow(tr("Log Level:"), m_traceLevelCombo);

    m_nmeaReqCombo = new QComboBox();
    m_nmeaReqCombo->addItem(tr("None"), 0);
    m_nmeaReqCombo->addItem(tr("GGA Only"), 1);
    m_nmeaReqCombo->addItem(tr("GGA + GSA + RMC + GSV"), 2);
    globalLayout->addRow(tr("NMEA Request:"), m_nmeaReqCombo);

    m_fswapMarginSpin = new QSpinBox();
    m_fswapMarginSpin->setRange(0, 1000);
    m_fswapMarginSpin->setSuffix(" Hz");
    globalLayout->addRow(tr("Frequency Swap Margin:"), m_fswapMarginSpin);

    m_relayBackSpin = new QSpinBox();
    m_relayBackSpin->setRange(0, 10);
    globalLayout->addRow(tr("Relay Back Mode:"), m_relayBackSpin);

    m_staIdSpin = new QSpinBox();
    m_staIdSpin->setRange(0, 9999);
    globalLayout->addRow(tr("Station ID:"), m_staIdSpin);

    m_staSelSpin = new QSpinBox();
    m_staSelSpin->setRange(0, 10);
    globalLayout->addRow(tr("Station Selection:"), m_staSelSpin);

    m_antTypeEdit = new QLineEdit();
    globalLayout->addRow(tr("Antenna Type:"), m_antTypeEdit);

    m_rcvTypeEdit = new QLineEdit();
    globalLayout->addRow(tr("Receiver Type:"), m_rcvTypeEdit);

    m_langCombo = new QComboBox();
    m_langCombo->addItem(QString::fromUtf8("English"), 0);
    m_langCombo->addItem(QString::fromUtf8("中文"), 1);
    globalLayout->addRow(tr("Language:"), m_langCombo);

    tabWidget->addTab(globalTab, tr("Global"));

    // Server Options Tab
    QWidget *optTab = new QWidget();
    QFormLayout *optLayout = new QFormLayout(optTab);

    QStringList optLabels = {
        tr("Server Option 0"),
        tr("Server Option 1"),
        tr("Server Option 2"),
        tr("Server Option 3 (DGPS buffer)"),
        tr("Server Option 4"),
        tr("Server Option 5")
    };

    for (int i = 0; i < 6; ++i) {
        m_srvOpt[i] = new QSpinBox();
        m_srvOpt[i]->setRange(0, 1000000);
        optLayout->addRow(optLabels[i] + ":", m_srvOpt[i]);
    }

    tabWidget->addTab(optTab, tr("Server Options"));

    // Paths Tab
    QWidget *pathTab = new QWidget();
    QFormLayout *pathLayout = new QFormLayout(pathTab);

    m_logFileEdit = new QLineEdit();
    pathLayout->addRow(tr("Log File:"), m_logFileEdit);

    m_srctblFileEdit = new QLineEdit();
    pathLayout->addRow(tr("Source Table File:"), m_srctblFileEdit);

    m_staposFileEdit = new QLineEdit();
    pathLayout->addRow(tr("Station Position File:"), m_staposFileEdit);

    m_proxyAddrEdit = new QLineEdit();
    m_proxyAddrEdit->setPlaceholderText(tr("http://proxy:port"));
    pathLayout->addRow(tr("Proxy Address:"), m_proxyAddrEdit);

    tabWidget->addTab(pathTab, tr("Paths"));

    // Record Tab
    QWidget *recordTab = new QWidget();
    QVBoxLayout *recordLayout = new QVBoxLayout(recordTab);

    m_recordEnableCheck = new QCheckBox(tr("Enable RTCM data recording"));
    recordLayout->addWidget(m_recordEnableCheck);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel(tr("Directory:")));
    m_recordDirEdit = new QLineEdit();
    dirLayout->addWidget(m_recordDirEdit, 1);
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseRecordDir);
    dirLayout->addWidget(browseBtn);
    recordLayout->addLayout(dirLayout);

    m_recordFileLabel = new QLabel(tr("Current file: --"));
    m_recordFileLabel->setWordWrap(true);
    recordLayout->addWidget(m_recordFileLabel);

    QLabel *recordHint = new QLabel(tr(
        "Data distributed to the outputs is saved as <UTC date>.rtcm3\n"
        "(e.g. 20261124.rtcm3). A new file is started when the UTC date changes."));
    recordHint->setWordWrap(true);
    recordHint->setStyleSheet("color: #7f8c8d;");
    recordLayout->addWidget(recordHint);
    recordLayout->addStretch();

    tabWidget->addTab(recordTab, tr("Record"));

    mainLayout->addWidget(tabWidget);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SettingsDialog::loadSettings()
{
    AppConfig &config = AppConfig::instance();

    m_traceLevelCombo->setCurrentIndex(config.getInt("set", "tracelevel", 0));
    m_nmeaReqCombo->setCurrentIndex(config.getInt("set", "nmeareq", 0));
    m_fswapMarginSpin->setValue(config.getInt("set", "fswapmargin", 30));
    m_relayBackSpin->setValue(config.getInt("set", "relayback", 0));
    m_staIdSpin->setValue(config.getInt("set", "staid", 0));
    m_staSelSpin->setValue(config.getInt("set", "stasel", 0));
    m_antTypeEdit->setText(config.getString("set", "anttype", ""));
    m_rcvTypeEdit->setText(config.getString("set", "rcvtype", ""));

    for (int i = 0; i < 6; ++i) {
        m_srvOpt[i]->setValue(config.getInt("set", QString("svropt_%1").arg(i), 0));
    }

    m_logFileEdit->setText(config.getString("file", "logfile", ""));
    m_srctblFileEdit->setText(config.getString("file", "srctblfile", ""));
    m_staposFileEdit->setText(config.getString("stapos", "staposfile", ""));
    m_proxyAddrEdit->setText(config.getString("dirs", "proxyaddress", ""));

    m_langCombo->setCurrentIndex(LanguageManager::instance().isChinese() ? 1 : 0);

    RtcmRecorder &rec = RtcmRecorder::instance();
    m_recordEnableCheck->setChecked(rec.isEnabled());
    m_recordDirEdit->setText(rec.directory());
    const QString currentFile = rec.currentFilePath();
    m_recordFileLabel->setText(currentFile.isEmpty()
                               ? tr("Current file: --")
                               : tr("Current file: %1").arg(currentFile));
}

void SettingsDialog::browseRecordDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Record Directory"),
                                                    m_recordDirEdit->text());
    if (!dir.isEmpty()) {
        m_recordDirEdit->setText(dir);
    }
}

void SettingsDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void SettingsDialog::saveSettings()
{
    AppConfig &config = AppConfig::instance();

    config.setInt("set", "tracelevel", m_traceLevelCombo->currentIndex());
    config.setInt("set", "nmeareq", m_nmeaReqCombo->currentIndex());
    config.setInt("set", "fswapmargin", m_fswapMarginSpin->value());
    config.setInt("set", "relayback", m_relayBackSpin->value());
    config.setInt("set", "staid", m_staIdSpin->value());
    config.setInt("set", "stasel", m_staSelSpin->value());
    config.setString("set", "anttype", m_antTypeEdit->text());
    config.setString("set", "rcvtype", m_rcvTypeEdit->text());

    for (int i = 0; i < 6; ++i) {
        config.setInt("set", QString("svropt_%1").arg(i), m_srvOpt[i]->value());
    }

    config.setString("file", "logfile", m_logFileEdit->text());
    config.setString("file", "srctblfile", m_srctblFileEdit->text());
    config.setString("stapos", "staposfile", m_staposFileEdit->text());
    config.setString("dirs", "proxyaddress", m_proxyAddrEdit->text());

    RtcmRecorder &rec = RtcmRecorder::instance();
    QString recDir = m_recordDirEdit->text().trimmed();
    if (recDir.isEmpty()) recDir = QDir::currentPath();
    rec.setDirectory(recDir);
    rec.setEnabled(m_recordEnableCheck->isChecked());
    rec.saveToConfig();

    LanguageManager::Language lang = (m_langCombo->currentIndex() == 1)
            ? LanguageManager::Chinese : LanguageManager::English;
    LanguageManager::instance().setLang(lang);

    config.save();
    LOG_INFO("Settings saved");
}
