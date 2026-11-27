#include "streammanager.h"
#include "../io/serialport.h"
#include "../io/network.h"
#include "logger.h"
#include "rtcmrecorder.h"

#include <QPair>
#include <QDir>

StreamManager *StreamManager::s_instance = nullptr;

StreamManager &StreamManager::instance()
{
    if (!s_instance) {
        s_instance = new StreamManager();
    }
    return *s_instance;
}

StreamManager::StreamManager(QObject *parent)
    : QObject(parent)
{
}

StreamManager::~StreamManager()
{
    stopAll();
}

bool StreamManager::startAll()
{
    QMutexLocker locker(&m_mutex);
    m_running = true;
    m_serialBytesLogged.clear();
    m_ggaLogged.clear();

    // 清掉各通道上次残留的 NMEA 尾部缓冲（Stop→Start 之间不会收到数据）
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        it.value().buffer.clear();
    }

    LOG_INFO("StreamManager starting...");

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it.value().enabled) {
            ChannelInfo &info = it.value();
            int id = it.key();

            switch (info.type) {
            case ChannelType::Serial:
                if (m_serialWorkers.contains(id) && m_serialWorkers[id]) {
                    m_serialWorkers[id]->open();
                }
                break;
            case ChannelType::TcpServer:
                if (m_tcpServerWorkers.contains(id) && m_tcpServerWorkers[id]) {
                    m_tcpServerWorkers[id]->start(info.port);
                }
                break;
            case ChannelType::TcpClient:
                if (m_tcpClientWorkers.contains(id) && m_tcpClientWorkers[id]) {
                    m_tcpClientWorkers[id]->start();
                }
                break;
            case ChannelType::UdpServer:
                if (m_udpServerWorkers.contains(id) && m_udpServerWorkers[id]) {
                    m_udpServerWorkers[id]->start(info.port);
                }
                break;
            case ChannelType::UdpClient:
                if (m_udpClientWorkers.contains(id) && m_udpClientWorkers[id]) {
                    m_udpClientWorkers[id]->start();
                }
                break;
            case ChannelType::Ntrip:
                if (m_ntripWorkers.contains(id) && m_ntripWorkers[id]) {
                    m_ntripWorkers[id]->start();
                }
                break;
            case ChannelType::File: {
                if (info.filePath.isEmpty()) {
                    LOG_ERROR("File channel #%d has no path", id);
                    break;
                }
                if (info.isOutput) {
                    QFile *f = new QFile(info.filePath);
                    if (f->open(QIODevice::WriteOnly | QIODevice::Append)) {
                        m_fileOutputs[id] = f;
                        LOG_INFO("File output opened: %s", info.filePath.toStdString().c_str());
                    } else {
                        LOG_ERROR("Cannot open output file %s", info.filePath.toStdString().c_str());
                        delete f;
                    }
                } else {
                    QFile *f = new QFile(info.filePath);
                    if (f->open(QIODevice::ReadOnly)) {
                        m_fileInputs[id] = f;
                        QTimer *t = new QTimer(this);
                        connect(t, &QTimer::timeout, this, [this, id]() { readFileChunk(id); });
                        t->start(100);
                        m_fileTimers[id] = t;
                        LOG_INFO("File input opened: %s", info.filePath.toStdString().c_str());
                    } else {
                        LOG_ERROR("Cannot open input file %s", info.filePath.toStdString().c_str());
                        delete f;
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    LOG_INFO("StreamManager started successfully");
    return true;
}

void StreamManager::stopAll()
{
    QMutexLocker locker(&m_mutex);
    if (!m_running) return;

    LOG_INFO("StreamManager stopping...");

    for (auto it = m_serialWorkers.begin(); it != m_serialWorkers.end(); ++it) {
        it.value()->close();
    }
    for (auto it = m_tcpServerWorkers.begin(); it != m_tcpServerWorkers.end(); ++it) {
        it.value()->stop();
    }
    for (auto it = m_tcpClientWorkers.begin(); it != m_tcpClientWorkers.end(); ++it) {
        it.value()->stop();
    }
    for (auto it = m_udpServerWorkers.begin(); it != m_udpServerWorkers.end(); ++it) {
        it.value()->stop();
    }
    for (auto it = m_udpClientWorkers.begin(); it != m_udpClientWorkers.end(); ++it) {
        it.value()->stop();
    }
    for (auto it = m_ntripWorkers.begin(); it != m_ntripWorkers.end(); ++it) {
        it.value()->stop();
    }

    for (auto it = m_fileTimers.begin(); it != m_fileTimers.end(); ++it) {
        it.value()->stop();
        it.value()->deleteLater();
    }
    m_fileTimers.clear();
    for (auto it = m_fileInputs.begin(); it != m_fileInputs.end(); ++it) {
        it.value()->close();
        delete it.value();
    }
    m_fileInputs.clear();
    for (auto it = m_fileOutputs.begin(); it != m_fileOutputs.end(); ++it) {
        it.value()->flush();
        it.value()->close();
        delete it.value();
    }
    m_fileOutputs.clear();

    m_running = false;
    LOG_INFO("StreamManager stopped");
}

bool StreamManager::isRunning() const
{
    QMutexLocker locker(&m_mutex);
    return m_running;
}

// ===== Channel Management =====

int StreamManager::addChannel(ChannelType type, const QString &name)
{
    QMutexLocker locker(&m_mutex);
    int id = m_nextChannelId++;

    ChannelInfo info;
    info.id = id;
    info.type = type;
    info.name = name.isEmpty() ? QString("Channel-%1").arg(id) : name;
    info.enabled = false;

    switch (type) {
    case ChannelType::Serial: {
        auto *worker = new SerialPortWorker(SerialPortWorker::Config(), this);
        connect(worker, &SerialPortWorker::dataReceived, this, &StreamManager::onSerialDataReceived);
        connect(worker, &SerialPortWorker::connected, this, &StreamManager::onSerialConnected);
        connect(worker, &SerialPortWorker::disconnected, this, &StreamManager::onSerialDisconnected);
        connect(worker, &SerialPortWorker::errorOccurred, this, &StreamManager::onSerialError);
        m_serialWorkers[id] = worker;
        info.description = QString("Serial Port");
        break;
    }
    case ChannelType::TcpServer: {
        auto *worker = new TcpServerWorker(0, this);
        connect(worker, &TcpServerWorker::dataReceived, this, &StreamManager::onTcpServerClientData);
        connect(worker, &TcpServerWorker::errorOccurred, this, &StreamManager::onTcpServerError);
        m_tcpServerWorkers[id] = worker;
        info.description = QString("TCP Server");
        break;
    }
    case ChannelType::TcpClient: {
        auto *worker = new TcpClientWorker(TcpClientWorker::Config(), this);
        connect(worker, &TcpClientWorker::dataReceived, this, &StreamManager::onTcpClientData);
        connect(worker, &TcpClientWorker::connected, this, &StreamManager::onTcpClientConnected);
        connect(worker, &TcpClientWorker::disconnected, this, &StreamManager::onTcpClientDisconnected);
        connect(worker, &TcpClientWorker::errorOccurred, this, &StreamManager::onTcpClientError);
        m_tcpClientWorkers[id] = worker;
        info.description = QString("TCP Client");
        break;
    }
    case ChannelType::UdpServer: {
        auto *worker = new UdpServerWorker(0, this);
        connect(worker, &UdpServerWorker::dataReceived, this, &StreamManager::onUdpServerData);
        m_udpServerWorkers[id] = worker;
        info.description = QString("UDP Server");
        break;
    }
    case ChannelType::UdpClient: {
        auto *worker = new UdpClientWorker(UdpClientWorker::Config(), this);
        m_udpClientWorkers[id] = worker;
        info.description = QString("UDP Client");
        break;
    }
    case ChannelType::Ntrip: {
        auto *worker = new NtripClientWorker(NtripClientWorker::Config(), this);
        connect(worker, &NtripClientWorker::dataReceived, this, &StreamManager::onNtripData);
        connect(worker, &NtripClientWorker::connected, this, [this, worker]() {
            int cid = m_ntripWorkers.key(worker, -1);
            if (cid >= 0) emit channelStarted(cid);
        });
        connect(worker, &NtripClientWorker::disconnected, this, [this, worker]() {
            int cid = m_ntripWorkers.key(worker, -1);
            if (cid >= 0) emit channelStopped(cid);
        });
        connect(worker, &NtripClientWorker::errorOccurred, this, [this, worker](const QString &err) {
            int cid = m_ntripWorkers.key(worker, -1);
            if (cid >= 0) emit channelError(cid, err);
        });
        m_ntripWorkers[id] = worker;
        info.description = QString("NTRIP Client");
        break;
    }
    case ChannelType::File:
        info.description = QString("File");
        break;
    default:
        break;
    }

    m_channels[id] = info;
    LOG_INFO("Added channel #%d: %s (%s)", id, info.name.toStdString().c_str(), info.description.toStdString().c_str());
    return id;
}

bool StreamManager::removeChannel(int id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(id)) return false;

    cleanupChannel(id);
    m_channels.remove(id);
    LOG_INFO("Removed channel #%d", id);
    return true;
}

bool StreamManager::enableChannel(int id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(id)) return false;
    m_channels[id].enabled = true;
    m_channels[id].lastActivity = QDateTime::currentDateTime();
    LOG_INFO("Enabled channel #%d", id);
    return true;
}

bool StreamManager::disableChannel(int id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(id)) return false;
    m_channels[id].enabled = false;
    LOG_INFO("Disabled channel #%d", id);
    return true;
}

ChannelInfo *StreamManager::channelInfo(int id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(id)) return nullptr;
    return &m_channels[id];
}

QList<int> StreamManager::channelIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_channels.keys();
}

// ===== Configuration =====

void StreamManager::configureSerial(int channelId, const SerialPortWorker::Config &config)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(channelId)) return;
    m_channels[channelId].serialPort = config.portName;
    m_channels[channelId].baudRate = config.baudRate;
    if (m_serialWorkers.contains(channelId) && m_serialWorkers[channelId]) {
        m_serialWorkers[channelId]->updateConfig(config);
    }
}

void StreamManager::configureTcpServer(int channelId, quint16 port)
{
    QMutexLocker locker(&m_mutex);
    if (!m_tcpServerWorkers.contains(channelId)) return;
    m_channels[channelId].port = port;
}

void StreamManager::configureTcpClient(int channelId, const TcpClientWorker::Config &config)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(channelId)) return;
    m_channels[channelId].host = config.host;
    m_channels[channelId].port = config.port;
    if (m_tcpClientWorkers.contains(channelId) && m_tcpClientWorkers[channelId]) {
        m_tcpClientWorkers[channelId]->updateConfig(config);
    }
}

void StreamManager::configureUdpServer(int channelId, quint16 port)
{
    QMutexLocker locker(&m_mutex);
    if (!m_udpServerWorkers.contains(channelId)) return;
    m_channels[channelId].port = port;
}

void StreamManager::configureUdpClient(int channelId, const UdpClientWorker::Config &config)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(channelId)) return;
    m_channels[channelId].host = config.host;
    m_channels[channelId].port = config.port;
    if (m_udpClientWorkers.contains(channelId) && m_udpClientWorkers[channelId]) {
        m_udpClientWorkers[channelId]->updateConfig(config);
    }
}

void StreamManager::configureNtrip(int channelId, const NtripClientWorker::Config &config)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(channelId)) return;
    m_channels[channelId].host = config.host;
    m_channels[channelId].port = config.port;
    if (m_ntripWorkers.contains(channelId) && m_ntripWorkers[channelId]) {
        m_ntripWorkers[channelId]->updateConfig(config);
    }
}

void StreamManager::configureFile(int channelId, const QString &path, bool asOutput)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(channelId)) return;
    m_channels[channelId].filePath = path;
    m_channels[channelId].isOutput = asOutput;
}

// ===== Routing =====

void StreamManager::addRoute(int inputId, int outputId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_channels.contains(inputId) || !m_channels.contains(outputId)) return;
    if (!m_routes[inputId].contains(outputId)) {
        m_routes[inputId].append(outputId);
        LOG_INFO("Route added: %d -> %d", inputId, outputId);
    }
}

void StreamManager::removeRoute(int inputId, int outputId)
{
    QMutexLocker locker(&m_mutex);
    m_routes[inputId].removeAll(outputId);
}

// ===== Signal Handlers =====

void StreamManager::onSerialDataReceived(const QByteArray &data)
{
    // Find channel that owns this worker
    SerialPortWorker *worker = qobject_cast<SerialPortWorker *>(sender());
    for (auto it = m_serialWorkers.begin(); it != m_serialWorkers.end(); ++it) {
        if (it.value() == worker) {
            const int id = it.key();
            if (!m_serialBytesLogged.contains(id)) {
                m_serialBytesLogged.insert(id);
                QString preview;
                const QByteArray head = data.left(48);
                for (int i = 0; i < head.size(); ++i) {
                    const unsigned char c = static_cast<unsigned char>(head.at(i));
                    preview += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
                }
                LOG_INFO("Serial channel #%d received %d bytes, preview: %s",
                         id, data.size(), preview.toStdString().c_str());
            }
            processData(id, data);
            return;
        }
    }
}

void StreamManager::onSerialConnected()
{
    emit channelStarted(m_serialWorkers.key(qobject_cast<SerialPortWorker *>(sender())));
}

void StreamManager::onSerialDisconnected()
{
    emit channelStopped(m_serialWorkers.key(qobject_cast<SerialPortWorker *>(sender())));
}

void StreamManager::onSerialError(const QString &error)
{
    auto *worker = qobject_cast<SerialPortWorker *>(sender());
    int id = m_serialWorkers.key(worker, -1);
    if (id >= 0) emit channelError(id, error);
}

void StreamManager::onTcpServerClientData(const QByteArray &data, const QString &from)
{
    Q_UNUSED(from);
    auto *worker = qobject_cast<TcpServerWorker *>(sender());
    int id = m_tcpServerWorkers.key(worker, -1);
    if (id >= 0) processData(id, data);
}

void StreamManager::onTcpServerError(const QString &error)
{
    auto *worker = qobject_cast<TcpServerWorker *>(sender());
    int id = m_tcpServerWorkers.key(worker, -1);
    if (id >= 0) emit channelError(id, error);
}

void StreamManager::onTcpClientData(const QByteArray &data)
{
    auto *worker = qobject_cast<TcpClientWorker *>(sender());
    int id = m_tcpClientWorkers.key(worker, -1);
    if (id >= 0) processData(id, data);
}

void StreamManager::onTcpClientConnected()
{
    auto *worker = qobject_cast<TcpClientWorker *>(sender());
    int id = m_tcpClientWorkers.key(worker, -1);
    if (id >= 0) emit channelStarted(id);
}

void StreamManager::onTcpClientDisconnected()
{
    auto *worker = qobject_cast<TcpClientWorker *>(sender());
    int id = m_tcpClientWorkers.key(worker, -1);
    if (id >= 0) emit channelStopped(id);
}

void StreamManager::onTcpClientError(const QString &error)
{
    auto *worker = qobject_cast<TcpClientWorker *>(sender());
    int id = m_tcpClientWorkers.key(worker, -1);
    if (id >= 0) emit channelError(id, error);
}

void StreamManager::onUdpServerData(const QByteArray &data, const QString &from, quint16 fromPort)
{
    Q_UNUSED(from);
    Q_UNUSED(fromPort);
    auto *worker = qobject_cast<UdpServerWorker *>(sender());
    int id = m_udpServerWorkers.key(worker, -1);
    if (id >= 0) processData(id, data);
}

void StreamManager::onNtripData(const QByteArray &data)
{
    auto *worker = qobject_cast<NtripClientWorker *>(sender());
    int id = m_ntripWorkers.key(worker, -1);
    if (id >= 0) processData(id, data);
}

// ===== Data Processing =====

void StreamManager::processData(int channelId, const QByteArray &data)
{
    QByteArray parseBuf;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_channels.contains(channelId)) return;
        m_channels[channelId].bytesReceived += data.size();
        m_channels[channelId].lastActivity = QDateTime::currentDateTime();

        // NMEA 流缓冲：把上次尾部残留拼上本次新数据，再按 '\n' 拆分。
        // 没有 '\n' 的尾部残留到 buffer，下次再来新数据时拼起来。
        // 这样即使串口/TCP/UDP 在一条 NMEA 语句中间分包，仍然能正确解析。
        QByteArray &buf = m_channels[channelId].buffer;
        buf.append(data);
        const int lastNl = buf.lastIndexOf('\n');
        if (lastNl >= 0) {
            parseBuf = buf.left(lastNl + 1);
            buf = buf.mid(lastNl + 1);
        } else {
            // 整段都是未完成的 NMEA 尾部，等下次再来
            return;
        }
    }

    emit streamReceived(data, channelId);

    // Route data（传本次原始字节即可，下游是流式的；bytesSent 按本次量统计）
    routeData(channelId, data);

    // Parse NMEA sentences from the reassembled buffer
    QList<QString> sentences = NmeaParser::extractSentences(parseBuf);
    for (const QString &sentence : sentences) {
        NmeaParser::SentenceType type = NmeaParser::parseSentenceType(sentence);
        if (type != NmeaParser::Unknown && NmeaParser::isValidChecksum(sentence)) {
            QMutexLocker locker(&m_mutex);
            if (m_channels.contains(channelId)) {
                m_channels[channelId].sentencesParsed++;
            }

            // Parse and update internal state
            switch (type) {
            case NmeaParser::GGA: {
                NmeaParser::GgaData gga;
                if (NmeaParser::parseGGA(sentence, gga)) {
                    if (!m_ggaLogged.contains(channelId)) {
                        m_ggaLogged.insert(channelId);
                        LOG_INFO("GGA parsed from channel #%d: lat=%.7f lon=%.7f quality=%d sats=%d",
                                 channelId, gga.latitude, gga.longitude,
                                 static_cast<int>(gga.fixQuality), gga.satellitesUsed);
                    }

                    QMutexLocker nl(&m_nmeaMutex);
                    m_lastGga = gga;
                    m_lastFixTime = QDateTime::currentDateTime();
                    nl.unlock();

                    if (gga.fixQuality != NmeaParser::Invalid) {
                        emit positionUpdated(
                            gga.latitude, gga.longitude, gga.altitudeMsl,
                            gga.satellitesUsed,
                            NmeaParser::fixQualityToString(gga.fixQuality)
                        );
                    }

                    // 把接收机的 GGA 经纬高提供给 NTRIP 客户端（VRS 位置）。
                    // 只要经纬度有效就转发，不要求已获得差分定位：
                    // 冷启动时接收机通常输出质量=0 的 GGA，而 VRS 正是靠这个概略位置来选择虚拟站。
                    const bool validPosition = qAbs(gga.latitude) > 1e-7 || qAbs(gga.longitude) > 1e-7;
                    if (validPosition) {
                        for (auto nit = m_ntripWorkers.begin(); nit != m_ntripWorkers.end(); ++nit) {
                            if (nit.value()) {
                                nit.value()->updateGgaPosition(
                                    gga.latitude, gga.longitude, gga.altitudeMsl);
                            }
                        }
                    }
                }
                break;
            }
            case NmeaParser::RMC: {
                NmeaParser::RmcData rmc;
                if (NmeaParser::parseRMC(sentence, rmc)) {
                    QMutexLocker nl(&m_nmeaMutex);
                    m_lastRmc = rmc;
                }
                break;
            }
            default:
                break;
            }
        } else if (type == NmeaParser::GGA) {
            // 诊断：GGA 存在但校验和不正确（部分设备不输出校验和/串口干扰）
            const int key = -channelId - 1;
            if (!m_ggaLogged.contains(key)) {
                m_ggaLogged.insert(key);
                LOG_WARN("GGA checksum invalid from channel #%d: %s",
                         channelId, sentence.toStdString().c_str());
            }
        }
    }
}

void StreamManager::routeData(int inputId, const QByteArray &data)
{
    QList<QPair<int, ChannelType> > targets;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_routes.contains(inputId)) return;

        const QList<int> outputs = m_routes[inputId];
        if (outputs.isEmpty()) return;

        for (int outputId : outputs) {
            if (!m_channels.contains(outputId)) continue;
            m_channels[outputId].bytesSent += data.size();
            targets.append(qMakePair(outputId, m_channels[outputId].type));
        }
    }

    // 记录下发的差分数据：每段输入数据只写一次，避免多路输出重复
    RtcmRecorder::instance().write(data);

    for (int i = 0; i < targets.size(); ++i) {
        const int outputId = targets[i].first;

        switch (targets[i].second) {
        case ChannelType::Serial:
            if (m_serialWorkers.contains(outputId) && m_serialWorkers[outputId]) {
                m_serialWorkers[outputId]->write(data);
            }
            break;
        case ChannelType::TcpServer:
            if (m_tcpServerWorkers.contains(outputId) && m_tcpServerWorkers[outputId]) {
                m_tcpServerWorkers[outputId]->broadcast(data);
            }
            break;
        case ChannelType::TcpClient:
            if (m_tcpClientWorkers.contains(outputId) && m_tcpClientWorkers[outputId]) {
                m_tcpClientWorkers[outputId]->write(data);
            }
            break;
        case ChannelType::UdpClient:
            if (m_udpClientWorkers.contains(outputId) && m_udpClientWorkers[outputId]) {
                m_udpClientWorkers[outputId]->write(data);
            }
            break;
        case ChannelType::File:
            if (m_fileOutputs.contains(outputId) && m_fileOutputs[outputId]) {
                m_fileOutputs[outputId]->write(data);
                m_fileOutputs[outputId]->flush();
            }
            break;
        default:
            break;
        }
    }
}

void StreamManager::readFileChunk(int channelId)
{
    QFile *f = m_fileInputs.value(channelId, nullptr);
    if (!f) return;

    QByteArray chunk = f->read(4096);
    if (chunk.isEmpty()) {
        // 到达文件末尾：停止该通道的定时器（可再次 Start 重新回放）
        QTimer *t = m_fileTimers.value(channelId, nullptr);
        if (t) t->stop();
        LOG_INFO("File input reached end: %s", f->fileName().toStdString().c_str());
        return;
    }
    processData(channelId, chunk);
}

void StreamManager::updateGnssState(int channelId)
{
    Q_UNUSED(channelId);
}

NmeaParser::GgaData StreamManager::lastGga() const
{
    QMutexLocker locker(&m_nmeaMutex);
    return m_lastGga;
}

NmeaParser::RmcData StreamManager::lastRmc() const
{
    QMutexLocker locker(&m_nmeaMutex);
    return m_lastRmc;
}

QDateTime StreamManager::lastFixTime() const
{
    QMutexLocker locker(&m_nmeaMutex);
    return m_lastFixTime;
}

void StreamManager::cleanupChannel(int id)
{
    if (m_serialWorkers.contains(id)) {
        m_serialWorkers[id]->close();
        m_serialWorkers[id]->deleteLater();
        m_serialWorkers.remove(id);
    }
    if (m_tcpServerWorkers.contains(id)) {
        m_tcpServerWorkers[id]->stop();
        m_tcpServerWorkers[id]->deleteLater();
        m_tcpServerWorkers.remove(id);
    }
    if (m_tcpClientWorkers.contains(id)) {
        m_tcpClientWorkers[id]->stop();
        m_tcpClientWorkers[id]->deleteLater();
        m_tcpClientWorkers.remove(id);
    }
    if (m_udpServerWorkers.contains(id)) {
        m_udpServerWorkers[id]->stop();
        m_udpServerWorkers[id]->deleteLater();
        m_udpServerWorkers.remove(id);
    }
    if (m_udpClientWorkers.contains(id)) {
        m_udpClientWorkers[id]->stop();
        m_udpClientWorkers[id]->deleteLater();
        m_udpClientWorkers.remove(id);
    }
    if (m_ntripWorkers.contains(id)) {
        m_ntripWorkers[id]->stop();
        m_ntripWorkers[id]->deleteLater();
        m_ntripWorkers.remove(id);
    }
    if (m_fileTimers.contains(id)) {
        m_fileTimers[id]->stop();
        m_fileTimers[id]->deleteLater();
        m_fileTimers.remove(id);
    }
    if (m_fileInputs.contains(id)) {
        m_fileInputs[id]->close();
        delete m_fileInputs[id];
        m_fileInputs.remove(id);
    }
    if (m_fileOutputs.contains(id)) {
        m_fileOutputs[id]->flush();
        m_fileOutputs[id]->close();
        delete m_fileOutputs[id];
        m_fileOutputs.remove(id);
    }
}
