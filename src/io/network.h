/**
 * Network - TCP/UDP client/server for STRSVR
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <QList>

// ============ TCP Server ============

class TcpServerWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpServerWorker(quint16 port = 0, QObject *parent = nullptr);
    ~TcpServerWorker() override;

    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    quint16 port() const;

    void broadcast(const QByteArray &data);
    int clientCount() const;

signals:
    void clientConnected(const QString &address);
    void clientDisconnected(const QString &address);
    void dataReceived(const QByteArray &data, const QString &fromAddress);
    void errorOccurred(const QString &error);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientReadyRead();

private:
    QTcpServer *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
};

// ============ TCP Client ============

class TcpClientWorker : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString host;
        quint16 port = 0;
        int reconnectIntervalMs = 3000;
        bool autoReconnect = true;
    };

    explicit TcpClientWorker(const Config &config, QObject *parent = nullptr);
    ~TcpClientWorker() override;

    void start();
    void stop();
    bool isConnected() const;
    bool write(const QByteArray &data);

    void updateConfig(const Config &config);

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void tryReconnect();

private:
    QTcpSocket *m_socket = nullptr;
    Config m_config;
    QTimer *m_reconnectTimer = nullptr;
    bool m_running = false;
};

// ============ NTRIP Client ============

class NtripClientWorker : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString host;
        quint16 port = 2101;
        QString mountpoint;
        QString user;
        QString password;
        int reconnectIntervalMs = 5000;
        bool autoReconnect = true;

        // VRS：上传 GGA 提供概略位置。默认使用接收机输入流中的 GGA，
        // 手填经纬度仅作为收不到 GGA 时的兜底值。
        bool sendGga = true;
        bool ggaFromSerial = true; // true=优先使用输入数据（接收机）中的 GGA 经纬度
        double ggaLat = 0.0;      // 纬度，度（北正）
        double ggaLon = 0.0;      // 经度，度（东正）
        double ggaHeight = 0.0;   // 高程，米
        int ggaIntervalMs = 1000;
    };

    explicit NtripClientWorker(const Config &config, QObject *parent = nullptr);
    ~NtripClientWorker() override;

    void start();
    void stop();
    bool isConnected() const;
    bool write(const QByteArray &data);

    void updateConfig(const Config &config);
    void updateGgaPosition(double lat, double lon, double height);

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void tryReconnect();

private:
    void sendGgaSentence();
    static QByteArray makeGgaSentence(double lat, double lon, double height);
    QTcpSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_ggaTimer = nullptr;
    Config m_config;
    bool m_running = false;
    bool m_headerDone = false;
    bool m_requestLogged = false;
    bool m_requestSent = false;
    QByteArray m_buffer;
};

// ============ UDP Server (broadcast receive) ============

class UdpServerWorker : public QObject
{
    Q_OBJECT

public:
    explicit UdpServerWorker(quint16 port = 0, QObject *parent = nullptr);
    ~UdpServerWorker() override;

    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    quint16 port() const;

signals:
    void dataReceived(const QByteArray &data, const QString &fromAddress, quint16 fromPort);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
};

// ============ UDP Client (broadcast send) ============

class UdpClientWorker : public QObject
{
    Q_OBJECT

public:
    struct Config {
        QString host;
        quint16 port = 0;
    };

    explicit UdpClientWorker(const Config &config, QObject *parent = nullptr);
    ~UdpClientWorker() override;

    bool start();
    void stop();
    bool isRunning() const;
    bool write(const QByteArray &data);
    void updateConfig(const Config &config);

private:
    QUdpSocket *m_socket = nullptr;
    Config m_config;
};

#endif // NETWORK_H
