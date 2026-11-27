/**
 * SerialPort - Serial port I/O for STRSVR
 *
 * Wraps QSerialPort with STRSVR-specific configuration and threading.
 */

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QStringList>
#include <QMutex>
#include <QList>
#include <QTimer>
#include <functional>

class SerialPortWorker : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString portName;          // e.g., /dev/ttyUSB0, /dev/ttyACM0, COM1
        qint32 baudRate = 115200;
        QSerialPort::DataBits dataBits = QSerialPort::Data8;
        QSerialPort::Parity parity = QSerialPort::NoParity;
        QSerialPort::StopBits stopBits = QSerialPort::OneStop;
        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;

        // Custom commands to send at startup
        QStringList startupCommands;

        // Timeouts
        int readTimeoutMs = 100;
        int writeTimeoutMs = 1000;
    };

    explicit SerialPortWorker(const Config &config, QObject *parent = nullptr);
    ~SerialPortWorker() override;

    bool open();
    void close();
    bool isOpen() const;

    bool write(const QByteArray &data);

    /** 更新配置（若已打开会先关闭，下次 open() 使用新参数）。 */
    void updateConfig(const Config &config);

    static QList<QSerialPortInfo> enumerate();
    static QStringList availablePorts();

signals:
    void dataReceived(const QByteArray &data);
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_serial;
    Config m_config;
    mutable QMutex m_mutex;
};

#endif // SERIALPORT_H
