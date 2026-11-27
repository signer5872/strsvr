#include "networkdialog.h"

NetworkDialog::NetworkDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Network Settings"));
    resize(500, 450);
    buildUi();
}

void NetworkDialog::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QTabWidget *tabWidget = new QTabWidget(this);
    m_tabWidget = tabWidget;

    // TCP Server Tab
    QWidget *tcpServerTab = new QWidget();
    QFormLayout *tcpSrvLayout = new QFormLayout(tcpServerTab);

    m_tcpServerPortSpin = new QSpinBox();
    m_tcpServerPortSpin->setRange(1, 65535);
    m_tcpServerPortSpin->setValue(5000);
    tcpSrvLayout->addRow(tr("Listen Port:"), m_tcpServerPortSpin);

    QLabel *tcpSrvHint = new QLabel(tr("TCP Server accepts multiple clients and broadcasts data to all connected clients."));
    tcpSrvHint->setWordWrap(true);
    tcpSrvHint->setStyleSheet("color: #7f8c8d;");
    tcpSrvLayout->addRow(tcpSrvHint);

    tabWidget->addTab(tcpServerTab, tr("TCP Server"));

    // TCP Client Tab
    QWidget *tcpClientTab = new QWidget();
    QFormLayout *tcpCliLayout = new QFormLayout(tcpClientTab);

    m_tcpClientHostEdit = new QLineEdit();
    m_tcpClientHostEdit->setPlaceholderText(tr("e.g., 192.168.1.100"));
    tcpCliLayout->addRow(tr("Host/IP:"), m_tcpClientHostEdit);

    m_tcpClientPortSpin = new QSpinBox();
    m_tcpClientPortSpin->setRange(1, 65535);
    m_tcpClientPortSpin->setValue(5000);
    tcpCliLayout->addRow(tr("Port:"), m_tcpClientPortSpin);

    m_tcpClientReconnectCheck = new QCheckBox(tr("Auto-reconnect on disconnect"));
    m_tcpClientReconnectCheck->setChecked(true);
    tcpCliLayout->addRow(m_tcpClientReconnectCheck);

    m_tcpClientReconnectIntervalSpin = new QSpinBox();
    m_tcpClientReconnectIntervalSpin->setRange(100, 60000);
    m_tcpClientReconnectIntervalSpin->setSuffix(" ms");
    m_tcpClientReconnectIntervalSpin->setValue(3000);
    tcpCliLayout->addRow(tr("Reconnect Interval:"), m_tcpClientReconnectIntervalSpin);

    tabWidget->addTab(tcpClientTab, tr("TCP Client"));

    // UDP Server Tab
    QWidget *udpServerTab = new QWidget();
    QFormLayout *udpSrvLayout = new QFormLayout(udpServerTab);

    m_udpServerPortSpin = new QSpinBox();
    m_udpServerPortSpin->setRange(1, 65535);
    m_udpServerPortSpin->setValue(5000);
    udpSrvLayout->addRow(tr("Listen Port:"), m_udpServerPortSpin);

    tabWidget->addTab(udpServerTab, tr("UDP Server"));

    // UDP Client Tab
    QWidget *udpClientTab = new QWidget();
    QFormLayout *udpCliLayout = new QFormLayout(udpClientTab);

    m_udpClientHostEdit = new QLineEdit();
    m_udpClientHostEdit->setPlaceholderText(tr("e.g., 224.0.0.1 (multicast)"));
    udpCliLayout->addRow(tr("Target Host:"), m_udpClientHostEdit);

    m_udpClientPortSpin = new QSpinBox();
    m_udpClientPortSpin->setRange(1, 65535);
    m_udpClientPortSpin->setValue(5000);
    udpCliLayout->addRow(tr("Target Port:"), m_udpClientPortSpin);

    tabWidget->addTab(udpClientTab, tr("UDP Client"));

    // NTRIP Tab
    QWidget *ntripTab = new QWidget();
    QFormLayout *ntripLayout = new QFormLayout(ntripTab);

    m_ntripHostEdit = new QLineEdit();
    m_ntripHostEdit->setPlaceholderText(tr("e.g., caster.igs.org"));
    ntripLayout->addRow(tr("Caster Host:"), m_ntripHostEdit);

    m_ntripPortSpin = new QSpinBox();
    m_ntripPortSpin->setRange(1, 65535);
    m_ntripPortSpin->setValue(2101);
    ntripLayout->addRow(tr("Caster Port:"), m_ntripPortSpin);

    m_ntripMountpointEdit = new QLineEdit();
    m_ntripMountpointEdit->setPlaceholderText(tr("e.g., ALGO0"));
    ntripLayout->addRow(tr("Mountpoint:"), m_ntripMountpointEdit);

    m_ntripUserEdit = new QLineEdit();
    m_ntripUserEdit->setPlaceholderText(tr("Username (optional)"));
    ntripLayout->addRow(tr("Username:"), m_ntripUserEdit);

    m_ntripPasswordEdit = new QLineEdit();
    m_ntripPasswordEdit->setEchoMode(QLineEdit::Password);
    m_ntripPasswordEdit->setPlaceholderText(tr("Password (optional)"));
    ntripLayout->addRow(tr("Password:"), m_ntripPasswordEdit);

    // VRS：认证后需周期性上传 GGA 才会下发差分（默认开启）
    m_ntripGgaCheck = new QCheckBox(tr("Upload GGA to caster (VRS)"));
    m_ntripGgaCheck->setChecked(true);
    ntripLayout->addRow(m_ntripGgaCheck);

    // 默认使用接收机输入流中的 GGA；下面的经纬度仅作为收不到 GGA 时的兜底
    m_ntripGgaAutoCheck = new QCheckBox(tr("Use GGA from input stream (auto)"));
    m_ntripGgaAutoCheck->setChecked(true);
    ntripLayout->addRow(m_ntripGgaAutoCheck);

    m_ntripLatEdit = new QLineEdit();
    m_ntripLatEdit->setPlaceholderText(tr("Fallback latitude (optional)"));
    ntripLayout->addRow(tr("Latitude (deg):"), m_ntripLatEdit);

    m_ntripLonEdit = new QLineEdit();
    m_ntripLonEdit->setPlaceholderText(tr("Fallback longitude (optional)"));
    ntripLayout->addRow(tr("Longitude (deg):"), m_ntripLonEdit);

    m_ntripHeightEdit = new QLineEdit();
    m_ntripHeightEdit->setPlaceholderText(tr("Fallback height (optional)"));
    ntripLayout->addRow(tr("Height (m):"), m_ntripHeightEdit);

    connect(m_ntripGgaAutoCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on) m_ntripGgaCheck->setChecked(true);
        updateGgaLatLonState();
    });
    connect(m_ntripGgaCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) m_ntripGgaAutoCheck->setChecked(false);
        updateGgaLatLonState();
    });
    updateGgaLatLonState();

    tabWidget->addTab(ntripTab, tr("NTRIP Client"));

    mainLayout->addWidget(tabWidget);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

TcpClientWorker::Config NetworkDialog::tcpClientConfig() const
{
    TcpClientWorker::Config cfg;
    cfg.host = m_tcpClientHostEdit->text();
    cfg.port = m_tcpClientPortSpin->value();
    cfg.autoReconnect = m_tcpClientReconnectCheck->isChecked();
    cfg.reconnectIntervalMs = m_tcpClientReconnectIntervalSpin->value();
    return cfg;
}

quint16 NetworkDialog::tcpServerPort() const
{
    return static_cast<quint16>(m_tcpServerPortSpin->value());
}

quint16 NetworkDialog::udpServerPort() const
{
    return static_cast<quint16>(m_udpServerPortSpin->value());
}

UdpClientWorker::Config NetworkDialog::udpClientConfig() const
{
    UdpClientWorker::Config cfg;
    cfg.host = m_udpClientHostEdit->text();
    cfg.port = static_cast<quint16>(m_udpClientPortSpin->value());
    return cfg;
}

NtripClientWorker::Config NetworkDialog::ntripConfig() const
{
    NtripClientWorker::Config cfg;
    cfg.host = m_ntripHostEdit->text();
    cfg.port = static_cast<quint16>(m_ntripPortSpin->value());
    cfg.mountpoint = m_ntripMountpointEdit->text();
    cfg.user = m_ntripUserEdit->text();
    cfg.password = m_ntripPasswordEdit->text();
    cfg.sendGga = m_ntripGgaCheck->isChecked();
    cfg.ggaFromSerial = m_ntripGgaAutoCheck->isChecked();
    cfg.ggaLat = m_ntripLatEdit->text().trimmed().toDouble();
    cfg.ggaLon = m_ntripLonEdit->text().trimmed().toDouble();
    cfg.ggaHeight = m_ntripHeightEdit->text().trimmed().toDouble();
    return cfg;
}

void NetworkDialog::setTcpServerPort(quint16 port)
{
    if (port > 0) m_tcpServerPortSpin->setValue(port);
}

void NetworkDialog::setUdpServerPort(quint16 port)
{
    if (port > 0) m_udpServerPortSpin->setValue(port);
}

void NetworkDialog::setTcpClientConfig(const TcpClientWorker::Config &cfg)
{
    m_tcpClientHostEdit->setText(cfg.host);
    if (cfg.port > 0) m_tcpClientPortSpin->setValue(cfg.port);
    m_tcpClientReconnectCheck->setChecked(cfg.autoReconnect);
    if (cfg.reconnectIntervalMs > 0) m_tcpClientReconnectIntervalSpin->setValue(cfg.reconnectIntervalMs);
}

void NetworkDialog::setUdpClientConfig(const UdpClientWorker::Config &cfg)
{
    m_udpClientHostEdit->setText(cfg.host);
    if (cfg.port > 0) m_udpClientPortSpin->setValue(cfg.port);
}

void NetworkDialog::setNtripConfig(const NtripClientWorker::Config &cfg)
{
    m_ntripHostEdit->setText(cfg.host);
    if (cfg.port > 0) m_ntripPortSpin->setValue(cfg.port);
    m_ntripMountpointEdit->setText(cfg.mountpoint);
    m_ntripUserEdit->setText(cfg.user);
    m_ntripPasswordEdit->setText(cfg.password);
    m_ntripGgaCheck->setChecked(cfg.sendGga);
    m_ntripGgaAutoCheck->setChecked(cfg.ggaFromSerial);
    m_ntripLatEdit->setText(cfg.ggaLat != 0.0 ? QString::number(cfg.ggaLat, 'f', 7) : QString());
    m_ntripLonEdit->setText(cfg.ggaLon != 0.0 ? QString::number(cfg.ggaLon, 'f', 7) : QString());
    m_ntripHeightEdit->setText(cfg.ggaHeight != 0.0 ? QString::number(cfg.ggaHeight, 'f', 1) : QString());
    updateGgaLatLonState();
}

void NetworkDialog::updateGgaLatLonState()
{
    // 经纬高框只有在「开启 GGA 上传」且「不自动解析」时才可用
    const bool enabled = m_ntripGgaCheck && m_ntripGgaCheck->isChecked()
                       && m_ntripGgaAutoCheck && !m_ntripGgaAutoCheck->isChecked();
    if (m_ntripLatEdit) m_ntripLatEdit->setEnabled(enabled);
    if (m_ntripLonEdit) m_ntripLonEdit->setEnabled(enabled);
    if (m_ntripHeightEdit) m_ntripHeightEdit->setEnabled(enabled);
}

void NetworkDialog::setCurrentPage(int index)
{
    if (m_tabWidget) m_tabWidget->setCurrentIndex(index);
}
