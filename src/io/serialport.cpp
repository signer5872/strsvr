#include "serialport.h"
#include "../core/logger.h"

#include <QDebug>

SerialPortWorker::SerialPortWorker(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    connect(&m_serial, &QSerialPort::readyRead, this, &SerialPortWorker::onReadyRead);
    // Qt 5.8 之前错误信号名为 error()，没有 errorOccurred()
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialPortWorker::onError);
#else
    connect(&m_serial, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error),
            this, &SerialPortWorker::onError);
#endif
}

SerialPortWorker::~SerialPortWorker()
{
    close();
}

bool SerialPortWorker::open()
{
    QMutexLocker locker(&m_mutex);

    if (m_serial.isOpen()) {
        LOG_WARN("Serial port already open: %s", m_config.portName.toStdString().c_str());
        return true;
    }

    m_serial.setPortName(m_config.portName);
    m_serial.setBaudRate(m_config.baudRate);
    m_serial.setDataBits(m_config.dataBits);
    m_serial.setParity(m_config.parity);
    m_serial.setStopBits(m_config.stopBits);
    m_serial.setFlowControl(m_config.flowControl);

    if (!m_serial.open(QIODevice::ReadWrite)) {
        QString err = m_serial.errorString();
        LOG_ERROR("Failed to open serial port %s: %s",
                  m_config.portName.toStdString().c_str(), err.toStdString().c_str());
        if (m_serial.error() == QSerialPort::PermissionError) {
            LOG_ERROR("Hint: add your user to the dialout group, e.g. "
                      "'sudo usermod -aG dialout $USER' then log in again");
        }
        emit errorOccurred(err);
        return false;
    }

    LOG_INFO("Serial port opened: %s @ %d baud",
             m_config.portName.toStdString().c_str(), m_config.baudRate);

    emit connected();

    // Send startup commands
    for (const QString &cmd : m_config.startupCommands) {
        if (!cmd.isEmpty()) {
            QByteArray raw = cmd.toLatin1();
            if (!raw.endsWith('\r')) raw += '\r';
            if (!raw.endsWith('\n')) raw += '\n';
            m_serial.write(raw);
            LOG_DEBUG("Sent startup command: %s", cmd.toStdString().c_str());
        }
    }

    return true;
}

void SerialPortWorker::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_serial.isOpen()) {
        LOG_INFO("Closing serial port: %s", m_config.portName.toStdString().c_str());
        m_serial.close();
    }
}

bool SerialPortWorker::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_serial.isOpen();
}

bool SerialPortWorker::write(const QByteArray &data)
{
    QMutexLocker locker(&m_mutex);
    if (!m_serial.isOpen()) return false;

    qint64 written = m_serial.write(data);
    if (written != data.size()) {
        LOG_WARN("Serial write incomplete: wrote %lld of %lld bytes",
                 written, data.size());
        return false;
    }

    m_serial.waitForBytesWritten(m_config.writeTimeoutMs);
    return true;
}

void SerialPortWorker::updateConfig(const Config &config)
{
    QMutexLocker locker(&m_mutex);
    if (m_serial.isOpen()) {
        m_serial.close();
    }
    m_config = config;
}

void SerialPortWorker::onReadyRead()
{
    QByteArray data = m_serial.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialPortWorker::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    QString errMsg = m_serial.errorString();
    LOG_ERROR("Serial port error (%d): %s", error, errMsg.toStdString().c_str());
    emit errorOccurred(errMsg);
}

QList<QSerialPortInfo> SerialPortWorker::enumerate()
{
    return QSerialPortInfo::availablePorts();
}

QStringList SerialPortWorker::availablePorts()
{
    QStringList result;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports) {
        result.append(port.portName());
    }
    return result;
}
