#include "serialdialog.h"

SerialDialog::SerialDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Serial Port Settings"));
    resize(450, 350);

    buildUi();
    refreshPorts();
}

void SerialDialog::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Port selection
    QGroupBox *portBox = new QGroupBox(tr("Port"), this);
    QHBoxLayout *portLayout = new QHBoxLayout(portBox);

    m_portCombo = new QComboBox(portBox);
    m_portCombo->setEditable(true);

    m_refreshBtn = new QPushButton(tr("Refresh"), portBox);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SerialDialog::refreshPorts);

    portLayout->addWidget(new QLabel(tr("Port:")));
    portLayout->addWidget(m_portCombo, 1);
    portLayout->addWidget(m_refreshBtn);

    mainLayout->addWidget(portBox);

    // Serial parameters
    QGroupBox *paramsBox = new QGroupBox(tr("Parameters"), this);
    QFormLayout *paramsLayout = new QFormLayout(paramsBox);

    m_baudCombo = new QComboBox();
    QList<int> baudRates = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (int r : baudRates) {
        m_baudCombo->addItem(QString("%1").arg(r), r);
    }
    int defaultBaud = m_baudCombo->findText(QStringLiteral("115200"));
    if (defaultBaud >= 0) {
        m_baudCombo->setCurrentIndex(defaultBaud);
    }
    paramsLayout->addRow(tr("Baud Rate:"), m_baudCombo);

    m_dataBitsCombo = new QComboBox();
    m_dataBitsCombo->addItem(tr("5 bits"), 5);
    m_dataBitsCombo->addItem(tr("6 bits"), 6);
    m_dataBitsCombo->addItem(tr("7 bits"), 7);
    m_dataBitsCombo->addItem(tr("8 bits"), 8);
    m_dataBitsCombo->setCurrentIndex(3);
    paramsLayout->addRow(tr("Data Bits:"), m_dataBitsCombo);

    m_parityCombo = new QComboBox();
    m_parityCombo->addItem(tr("None"), 0);
    m_parityCombo->addItem(tr("Even"), 2);
    m_parityCombo->addItem(tr("Odd"), 3);
    m_parityCombo->addItem(tr("Mark"), 4);
    m_parityCombo->addItem(tr("Space"), 5);
    paramsLayout->addRow(tr("Parity:"), m_parityCombo);

    m_stopBitsCombo = new QComboBox();
    m_stopBitsCombo->addItem(tr("1 bit"), 1);
    m_stopBitsCombo->addItem(tr("1.5 bits"), 3);
    m_stopBitsCombo->addItem(tr("2 bits"), 2);
    paramsLayout->addRow(tr("Stop Bits:"), m_stopBitsCombo);

    m_flowControlCombo = new QComboBox();
    m_flowControlCombo->addItem(tr("None"), 0);
    m_flowControlCombo->addItem(tr("RTS/CTS"), 1);
    m_flowControlCombo->addItem(tr("XON/XOFF"), 2);
    paramsLayout->addRow(tr("Flow Control:"), m_flowControlCombo);

    mainLayout->addWidget(paramsBox);

    // Startup commands
    QGroupBox *cmdBox = new QGroupBox(tr("Startup Commands"), this);
    QVBoxLayout *cmdLayout = new QVBoxLayout(cmdBox);

    m_startupCmdsEdit = new QLineEdit(cmdBox);
    m_startupCmdsEdit->setPlaceholderText(tr("Comma-separated commands, e.g. $PCAS03,1,0,0,1,0*15"));
    cmdLayout->addWidget(m_startupCmdsEdit);

    QLabel *hint = new QLabel(tr("Commands will be sent when the port opens.\nUse \\r and \\n for line endings."));
    hint->setStyleSheet("color: #7f8c8d;");
    cmdLayout->addWidget(hint);

    mainLayout->addWidget(cmdBox);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SerialDialog::refreshPorts()
{
    m_portCombo->clear();
    QStringList ports = SerialPortWorker::availablePorts();
    m_portCombo->addItems(ports);

    // Common Linux defaults
    QStringList defaults = {"/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0",
                            "/dev/ttyACM1", "/dev/ttyS0", "/dev/ttyS1"};
    for (const QString &d : defaults) {
        if (!ports.contains(d)) {
            m_portCombo->addItem(d);
        }
    }
}

SerialPortWorker::Config SerialDialog::config() const
{
    SerialPortWorker::Config cfg;

    cfg.portName = m_portCombo->currentText();
    cfg.baudRate = m_baudCombo->currentData().toInt();

    switch (m_dataBitsCombo->currentData().toInt()) {
    case 5: cfg.dataBits = QSerialPort::Data5; break;
    case 6: cfg.dataBits = QSerialPort::Data6; break;
    case 7: cfg.dataBits = QSerialPort::Data7; break;
    default: cfg.dataBits = QSerialPort::Data8; break;
    }

    switch (m_parityCombo->currentData().toInt()) {
    case 2: cfg.parity = QSerialPort::EvenParity; break;
    case 3: cfg.parity = QSerialPort::OddParity; break;
    case 4: cfg.parity = QSerialPort::MarkParity; break;
    case 5: cfg.parity = QSerialPort::SpaceParity; break;
    default: cfg.parity = QSerialPort::NoParity; break;
    }

    switch (m_stopBitsCombo->currentData().toInt()) {
    case 2: cfg.stopBits = QSerialPort::TwoStop; break;
    case 3: cfg.stopBits = QSerialPort::OneAndHalfStop; break;
    default: cfg.stopBits = QSerialPort::OneStop; break;
    }

    switch (m_flowControlCombo->currentData().toInt()) {
    case 1: cfg.flowControl = QSerialPort::HardwareControl; break;
    case 2: cfg.flowControl = QSerialPort::SoftwareControl; break;
    default: cfg.flowControl = QSerialPort::NoFlowControl; break;
    }

    if (!m_startupCmdsEdit->text().isEmpty()) {
        cfg.startupCommands = m_startupCmdsEdit->text().split(',');
    }

    return cfg;
}

void SerialDialog::setConfig(const SerialPortWorker::Config &cfg)
{
    if (!cfg.portName.isEmpty()) {
        int idx = m_portCombo->findText(cfg.portName);
        if (idx < 0) {
            m_portCombo->addItem(cfg.portName);
            idx = m_portCombo->count() - 1;
        }
        m_portCombo->setCurrentIndex(idx);
    }

    int bi = m_baudCombo->findData(cfg.baudRate);
    if (bi < 0) {
        m_baudCombo->addItem(QString::number(cfg.baudRate), cfg.baudRate);
        bi = m_baudCombo->count() - 1;
    }
    m_baudCombo->setCurrentIndex(bi);

    int dataBits = 8;
    switch (cfg.dataBits) {
    case QSerialPort::Data5: dataBits = 5; break;
    case QSerialPort::Data6: dataBits = 6; break;
    case QSerialPort::Data7: dataBits = 7; break;
    default: dataBits = 8; break;
    }
    int di = m_dataBitsCombo->findData(dataBits);
    if (di >= 0) m_dataBitsCombo->setCurrentIndex(di);

    int parity = 0;
    switch (cfg.parity) {
    case QSerialPort::EvenParity: parity = 2; break;
    case QSerialPort::OddParity: parity = 3; break;
    case QSerialPort::MarkParity: parity = 4; break;
    case QSerialPort::SpaceParity: parity = 5; break;
    default: parity = 0; break;
    }
    int pi = m_parityCombo->findData(parity);
    if (pi >= 0) m_parityCombo->setCurrentIndex(pi);

    int stopBits = 1;
    switch (cfg.stopBits) {
    case QSerialPort::TwoStop: stopBits = 2; break;
    case QSerialPort::OneAndHalfStop: stopBits = 3; break;
    default: stopBits = 1; break;
    }
    int si = m_stopBitsCombo->findData(stopBits);
    if (si >= 0) m_stopBitsCombo->setCurrentIndex(si);

    int flow = 0;
    switch (cfg.flowControl) {
    case QSerialPort::HardwareControl: flow = 1; break;
    case QSerialPort::SoftwareControl: flow = 2; break;
    default: flow = 0; break;
    }
    int fi = m_flowControlCombo->findData(flow);
    if (fi >= 0) m_flowControlCombo->setCurrentIndex(fi);

    m_startupCmdsEdit->setText(cfg.startupCommands.join(","));
}
