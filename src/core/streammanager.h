/**
 * StreamManager - Central data flow manager for STRSVR
 *
 * Orchestrates all input/output streams, routing data between
 * serial ports, TCP/UDP sockets, files, NTRIP clients, etc.
 */

#ifndef STREAMMANAGER_H
#define STREAMMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QSet>

#include "io/serialport.h"
#include "io/network.h"
#include "core/nmeaparser.h"

// Channel identifiers
enum class ChannelType {
    None = 0,
    Serial = 1,
    TcpServer = 2,
    TcpClient = 3,
    UdpServer = 4,
    UdpClient = 5,
    File = 6,
    Ntrip = 7
};

struct ChannelInfo {
    int id = -1;
    ChannelType type = ChannelType::None;
    QString name;
    QString description;
    bool enabled = false;
    QByteArray buffer;           // Data buffer
    quint64 bytesReceived = 0;
    quint64 bytesSent = 0;
    quint64 sentencesParsed = 0;
    QDateTime lastActivity;

    // Type-specific config
    QString serialPort;
    int baudRate = 115200;
    QString host;
    quint16 port = 0;
    QString filePath;
    bool isOutput = false;       // File 通道：true=写文件, false=读文件
};

class StreamManager : public QObject
{
    Q_OBJECT

public:
    static StreamManager &instance();

    // Lifecycle
    bool startAll();
    void stopAll();
    bool isRunning() const;

    // Channel management
    int addChannel(ChannelType type, const QString &name = "");
    bool removeChannel(int id);
    bool enableChannel(int id);
    bool disableChannel(int id);
    ChannelInfo *channelInfo(int id);
    QList<int> channelIds() const;

    // Configuration
    void configureSerial(int channelId, const SerialPortWorker::Config &config);
    void configureTcpServer(int channelId, quint16 port);
    void configureTcpClient(int channelId, const TcpClientWorker::Config &config);
    void configureUdpServer(int channelId, quint16 port);
    void configureUdpClient(int channelId, const UdpClientWorker::Config &config);
    void configureNtrip(int channelId, const NtripClientWorker::Config &config);
    void configureFile(int channelId, const QString &path, bool asOutput);

    // Data routing
    // Route data from input channel to output channel
    void addRoute(int inputId, int outputId);
    void removeRoute(int inputId, int outputId);

    // Get GNSS data snapshot (for monitoring display)
    NmeaParser::GgaData lastGga() const;
    NmeaParser::RmcData lastRmc() const;
    QDateTime lastFixTime() const;

signals:
    void channelStarted(int channelId);
    void channelStopped(int channelId);
    void channelError(int channelId, const QString &error);
    void dataFlowUpdated(int channelId, quint64 bytesIn, quint64 bytesOut);
    void positionUpdated(double lat, double lon, double altitude, int satellites, QString fixQuality);
    void streamReceived(const QByteArray &data, int channelId);

private slots:
    void onSerialDataReceived(const QByteArray &data);
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialError(const QString &error);

    void onTcpServerClientData(const QByteArray &data, const QString &from);
    void onTcpServerError(const QString &error);

    void onTcpClientData(const QByteArray &data);
    void onTcpClientConnected();
    void onTcpClientDisconnected();
    void onTcpClientError(const QString &error);

    void onUdpServerData(const QByteArray &data, const QString &from, quint16 fromPort);
    void onNtripData(const QByteArray &data);

private:
    explicit StreamManager(QObject *parent = nullptr);
    ~StreamManager() override;

    static StreamManager *s_instance;

    mutable QMutex m_mutex;
    bool m_running = false;

    int m_nextChannelId = 0;
    QMap<int, ChannelInfo> m_channels;

    // Worker objects for each channel type
    QMap<int, SerialPortWorker *> m_serialWorkers;
    QMap<int, TcpServerWorker *> m_tcpServerWorkers;
    QMap<int, TcpClientWorker *> m_tcpClientWorkers;
    QMap<int, UdpServerWorker *> m_udpServerWorkers;
    QMap<int, UdpClientWorker *> m_udpClientWorkers;
    QMap<int, NtripClientWorker *> m_ntripWorkers;

    // File input/output channels
    QMap<int, QFile *> m_fileInputs;
    QMap<int, QFile *> m_fileOutputs;
    QMap<int, QTimer *> m_fileTimers;

    // Routing table: inputId -> list of outputIds
    QMap<int, QList<int>> m_routes;

    // Last parsed GNSS data
    mutable QMutex m_nmeaMutex;
    NmeaParser::GgaData m_lastGga;
    NmeaParser::RmcData m_lastRmc;
    QDateTime m_lastFixTime;

    // 诊断：每个通道首次收到数据/首次解析出 GGA 时打印一次
    QSet<int> m_serialBytesLogged;
    QSet<int> m_ggaLogged;

    // Parse incoming data and extract NMEA sentences
    void processData(int channelId, const QByteArray &data);

    // Forward data through routing table
    void routeData(int inputId, const QByteArray &data);

    // Update internal GNSS state
    void updateGnssState(int channelId);

    // File input timer callback
    void readFileChunk(int channelId);

    void cleanupChannel(int id);
};

#endif // STREAMMANAGER_H
