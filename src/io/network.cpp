#include "network.h"
#include "../core/logger.h"

#include <QDateTime>

// ============================================================
// TcpServerWorker
// ============================================================

TcpServerWorker::TcpServerWorker(quint16 port, QObject *parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TcpServerWorker::onNewConnection);
}

TcpServerWorker::~TcpServerWorker()
{
    stop();
}

bool TcpServerWorker::start(quint16 port)
{
    if (m_server->isListening()) {
        LOG_WARN("TCP server already listening on port %d", m_server->serverPort());
        return true;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        QString err = m_server->errorString();
        LOG_ERROR("Failed to start TCP server on port %d: %s", port, err.toStdString().c_str());
        emit errorOccurred(err);
        return false;
    }

    LOG_INFO("TCP server started on port %d", m_server->serverPort());
    return true;
}

void TcpServerWorker::stop()
{
    for (auto *client : m_clients) {
        client->close();
        client->deleteLater();
    }
    m_clients.clear();

    if (m_server->isListening()) {
        m_server->close();
        LOG_INFO("TCP server stopped");
    }
}

bool TcpServerWorker::isRunning() const
{
    return m_server->isListening();
}

quint16 TcpServerWorker::port() const
{
    return m_server->serverPort();
}

void TcpServerWorker::broadcast(const QByteArray &data)
{
    for (auto *client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
        }
    }
}

int TcpServerWorker::clientCount() const
{
    return m_clients.size();
}

void TcpServerWorker::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *client = m_server->nextPendingConnection();
        m_clients.append(client);

        QString address = client->peerAddress().toString();
        LOG_INFO("TCP client connected: %s", address.toStdString().c_str());

        connect(client, &QTcpSocket::disconnected, this, &TcpServerWorker::onClientDisconnected);
        connect(client, &QTcpSocket::readyRead, this, &TcpServerWorker::onClientReadyRead);

        emit clientConnected(address);
    }
}

void TcpServerWorker::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) return;

    QString address = client->peerAddress().toString();
    LOG_INFO("TCP client disconnected: %s", address.toStdString().c_str());

    m_clients.removeAll(client);
    client->deleteLater();

    emit clientDisconnected(address);
}

void TcpServerWorker::onClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) return;

    QByteArray data = client->readAll();
    emit dataReceived(data, client->peerAddress().toString());
}

// ============================================================
// TcpClientWorker
// ============================================================

TcpClientWorker::TcpClientWorker(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected, this, &TcpClientWorker::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClientWorker::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClientWorker::onReadyRead);
    // 用 static_cast 取代 QOverload，兼容 Qt 5.5（QOverload 需要 5.7+）
    connect(m_socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &TcpClientWorker::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClientWorker::tryReconnect);
}

TcpClientWorker::~TcpClientWorker()
{
    stop();
}

void TcpClientWorker::start()
{
    m_running = true;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->connectToHost(m_config.host, m_config.port);
    LOG_INFO("TCP client connecting to %s:%d",
             m_config.host.toStdString().c_str(), m_config.port);
}

void TcpClientWorker::stop()
{
    m_running = false;
    m_reconnectTimer->stop();
    // 不阻塞等待：在任意状态直接关闭，避免 waitForDisconnected() 在非连接态报警告
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    LOG_INFO("TCP client stopped");
}

bool TcpClientWorker::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool TcpClientWorker::write(const QByteArray &data)
{
    if (!isConnected()) return false;
    m_socket->write(data);
    return true;
}

void TcpClientWorker::updateConfig(const Config &config)
{
    m_config = config;
}

void TcpClientWorker::onConnected()
{
    LOG_INFO("TCP client connected to %s:%d",
             m_config.host.toStdString().c_str(), m_config.port);
    m_reconnectTimer->stop();
    emit connected();
}

void TcpClientWorker::onDisconnected()
{
    LOG_WARN("TCP client disconnected from %s:%d",
             m_config.host.toStdString().c_str(), m_config.port);
    emit disconnected();

    if (m_running && m_config.autoReconnect) {
        m_reconnectTimer->start(m_config.reconnectIntervalMs);
    }
}

void TcpClientWorker::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    emit dataReceived(data);
}

void TcpClientWorker::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString err = m_socket->errorString();
    LOG_ERROR("TCP client error: %s", err.toStdString().c_str());
    emit errorOccurred(err);
}

void TcpClientWorker::tryReconnect()
{
    if (!m_running) return;
    LOG_INFO("TCP client reconnecting to %s:%d...",
             m_config.host.toStdString().c_str(), m_config.port);
    m_socket->connectToHost(m_config.host, m_config.port);
}

// ============================================================
// NtripClientWorker
// ============================================================

NtripClientWorker::NtripClientWorker(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    m_ggaTimer = new QTimer(this);
    m_ggaTimer->setSingleShot(false);

    connect(m_socket, &QTcpSocket::connected, this, &NtripClientWorker::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NtripClientWorker::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NtripClientWorker::onReadyRead);
    connect(m_socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &NtripClientWorker::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &NtripClientWorker::tryReconnect);
    connect(m_ggaTimer, &QTimer::timeout, this, &NtripClientWorker::sendGgaSentence);
}

NtripClientWorker::~NtripClientWorker()
{
    stop();
}

void NtripClientWorker::start()
{
    m_running = true;
    m_headerDone = false;
    m_requestLogged = false;
    m_requestSent = false;
    m_buffer.clear();
    m_ggaTimer->stop();
    if (m_config.mountpoint.trimmed().isEmpty()) {
        m_running = false;
        QString err("NTRIP mountpoint is empty");
        LOG_ERROR("%s", err.toStdString().c_str());
        emit errorOccurred(err);
        return;
    }
    if (m_config.sendGga && !m_config.ggaFromSerial
            && qAbs(m_config.ggaLat) < 1e-7 && qAbs(m_config.ggaLon) < 1e-7) {
        m_running = false;
        QString err("NTRIP GGA position is not set (latitude/longitude are 0)");
        LOG_ERROR("%s", err.toStdString().c_str());
        emit errorOccurred(err);
        return;
    }
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->connectToHost(m_config.host, m_config.port);
    LOG_INFO("NTRIP client connecting to %s:%d/%s",
             m_config.host.toStdString().c_str(), m_config.port,
             m_config.mountpoint.toStdString().c_str());
}

void NtripClientWorker::stop()
{
    m_running = false;
    m_requestSent = false;
    m_reconnectTimer->stop();
    m_ggaTimer->stop();
    // 不阻塞等待：在任意状态直接关闭，避免 waitForDisconnected() 在非连接态报警告
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    LOG_INFO("NTRIP client stopped");
}

bool NtripClientWorker::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool NtripClientWorker::write(const QByteArray &data)
{
    if (!isConnected()) return false;
    m_socket->write(data);
    return true;
}

void NtripClientWorker::updateConfig(const Config &config)
{
    m_config = config;
}

void NtripClientWorker::updateGgaPosition(double lat, double lon, double height)
{
    if (!m_config.sendGga || !m_config.ggaFromSerial) return;
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return;

    const bool hadPosition = qAbs(m_config.ggaLat) > 1e-7 || qAbs(m_config.ggaLon) > 1e-7;

    m_config.ggaLat = lat;
    m_config.ggaLon = lon;
    m_config.ggaHeight = height;

    // 连接已建立且请求已发出时立即上传，不必等 200 响应头
    if (m_requestSent) {
        if (!hadPosition) {
            LOG_INFO("NTRIP GGA position from input stream: lat=%.7f lon=%.7f h=%.1f",
                     lat, lon, height);
        }
        sendGgaSentence();
    }
}

void NtripClientWorker::onConnected()
{
    // 去掉粘贴时可能带上的空白/回车，以及多余的 '/'，避免请求行非法被 Caster 直接断开
    QString mp = m_config.mountpoint.trimmed();
    while (mp.startsWith('/')) mp.remove(0, 1);
    const QString host = m_config.host.trimmed();

    QByteArray req;
    // 与 curl 成功拿到响应时完全一致的 NTRIP 2.0 请求
    req += "GET /" + mp.toLatin1() + " HTTP/1.1\r\n";
    req += "Host: " + host.toLatin1() + ":" + QByteArray::number(m_config.port) + "\r\n";
    if (!m_config.user.isEmpty() || !m_config.password.isEmpty()) {
        QByteArray auth = (m_config.user.trimmed() + ":" + m_config.password.trimmed()).toLatin1().toBase64();
        req += "Authorization: Basic " + auth + "\r\n";
    }
    req += "Accept: */*\r\n";
    req += "Ntrip-Version: Ntrip/2.0\r\n";
    req += "User-Agent: NTRIP STRSVR/2.4.3\r\n";
    req += "\r\n";
    m_socket->write(req);
    m_socket->flush();
    m_requestSent = true;

    LOG_INFO("NTRIP request sent: GET /%s HTTP/1.1 (mountpoint bytes=%s)",
             mp.toStdString().c_str(), mp.toLatin1().toHex().constData());

    // 首次连接打印完整请求（隐藏认证内容），便于和 curl 逐字节对比
    if (!m_requestLogged) {
        m_requestLogged = true;
        QByteArray dbg = req;
        const int authPos = dbg.indexOf("Authorization: Basic ");
        if (authPos >= 0) {
            const int valueStart = authPos + 21;
            const int valueEnd = dbg.indexOf("\r\n", valueStart);
            if (valueEnd > valueStart) {
                dbg.replace(valueStart, valueEnd - valueStart, "***");
            }
        }
        LOG_INFO("NTRIP request bytes: %s", dbg.toHex().constData());
    }

    // VRS：请求发出后立即开始周期上传 GGA（不等 200，否则会与 Caster 互等）
    if (m_config.sendGga) {
        const int interval = m_config.ggaIntervalMs > 0 ? m_config.ggaIntervalMs : 1000;
        m_ggaTimer->start(interval);
        LOG_INFO("NTRIP GGA upload enabled (%s), interval=%dms",
                 m_config.ggaFromSerial
                     ? ((qAbs(m_config.ggaLat) > 1e-7 || qAbs(m_config.ggaLon) > 1e-7)
                            ? "receiver GGA with fallback position"
                            : "waiting for receiver GGA")
                     : "manual",
                 interval);
        sendGgaSentence();
    }

    m_reconnectTimer->stop();
    emit connected();
}

void NtripClientWorker::onDisconnected()
{
    m_ggaTimer->stop();

    if (!m_headerDone && !m_buffer.isEmpty()) {
        // 服务端在返回完整响应头之前就断开：打印原始内容，便于判断 401/404/VRS 等问题
        LOG_WARN("NTRIP pending response (%d bytes), hex: %s",
                 m_buffer.size(), m_buffer.left(160).toHex().constData());
    }

    LOG_WARN("NTRIP client disconnected from %s:%d",
             m_config.host.toStdString().c_str(), m_config.port);
    emit disconnected();

    if (m_running && m_config.autoReconnect) {
        m_reconnectTimer->start(m_config.reconnectIntervalMs);
    }
}

void NtripClientWorker::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    if (data.isEmpty()) return;

    if (m_headerDone) {
        emit dataReceived(data);
        return;
    }

    m_buffer.append(data);

    // 找到响应头结束位置：
    //  - NTRIP 2.0/HTTP：以空行 \r\n\r\n 结束
    //  - NTRIP 1.0：响应为 "ICY 200 OK\r\n"，没有空行，首行结束即数据开始
    //  - 部分 Caster 出错时只回一行 "HTTP/1.0 401 Unauthorized\r\n" 就关连接，
    //    此时也要立即按错误处理，否则一直等空行会卡在 buffer 里无限重连
    int bodyStart = -1;
    QByteArray header;
    const int pos = m_buffer.indexOf("\r\n\r\n");
    if (pos >= 0) {
        header = m_buffer.left(pos);
        bodyStart = pos + 4;
    } else if (m_buffer.startsWith("ICY") && m_buffer.contains("\r\n")) {
        const int eol = m_buffer.indexOf("\r\n");
        header = m_buffer.left(eol);
        bodyStart = eol + 2;
    } else {
        // 首行完整且不是 200：错误响应可能只有一行，立即处理
        const int eol = m_buffer.indexOf("\r\n");
        if (eol > 0) {
            const QByteArray firstLine = m_buffer.left(eol);
            const bool looksHttp = firstLine.startsWith("HTTP/") || firstLine.startsWith("ICY");
            const bool ok200 = firstLine.contains(" 200") || firstLine.startsWith("ICY 200");
            if (looksHttp && !ok200) {
                header = firstLine;
                bodyStart = eol + 2;
            }
        }
    }

    if (bodyStart < 0) {
        // 防止异常服务端一直返回超长头部
        if (m_buffer.size() > 16384) {
            QString err = "NTRIP response header too large";
            LOG_ERROR("%s", err.toStdString().c_str());
            emit errorOccurred(err);
            stop();
        }
        return;
    }

    QByteArray body = m_buffer.mid(bodyStart);
    m_buffer.clear();

    const int eol = header.indexOf('\r');
    const QByteArray firstLine = header.left(eol >= 0 ? eol : header.size());
    const bool accepted = firstLine.contains(" 200") || firstLine.startsWith("ICY 200");

    if (accepted) {
        m_headerDone = true;
        LOG_INFO("NTRIP stream started: %s", firstLine.toStdString().c_str());
        if (!body.isEmpty()) emit dataReceived(body);
    } else {
        QString err = QString::fromLatin1(firstLine);
        LOG_ERROR("NTRIP request failed: %s", err.toStdString().c_str());
        emit errorOccurred(err);
        // 认证/挂载点错误重连也无意义：停止并关闭，但不阻塞等待
        m_running = false;
        m_headerDone = false;
        m_socket->abort();
    }
}

void NtripClientWorker::onError(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::RemoteHostClosedError) {
        // 服务端主动断开属于常见情况，由 onDisconnected() 负责重连，不重复报错
        LOG_WARN("NTRIP client: remote host closed connection");
        return;
    }

    QString err = m_socket->errorString();
    LOG_ERROR("NTRIP client error: %s", err.toStdString().c_str());
    emit errorOccurred(err);
}

void NtripClientWorker::tryReconnect()
{
    if (!m_running) return;
    LOG_INFO("NTRIP client reconnecting to %s:%d...",
             m_config.host.toStdString().c_str(), m_config.port);
    m_headerDone = false;
    m_requestLogged = false;
    m_requestSent = false;
    m_buffer.clear();
    m_socket->connectToHost(m_config.host, m_config.port);
}

void NtripClientWorker::sendGgaSentence()
{
    // 注意：不能等响应头。部分 VRS Caster 在收到 GGA 之前既不返回 200 也不推流
    if (!m_running || !m_requestSent) return;
    if (m_socket->state() != QAbstractSocket::ConnectedState) return;

    // 使用输入流 GGA 时，还没收到有效定位就先不发（避免把 0,0 位置传给 Caster）
    if (m_config.ggaFromSerial
            && qAbs(m_config.ggaLat) < 1e-7 && qAbs(m_config.ggaLon) < 1e-7) {
        return;
    }

    const QByteArray sentence = makeGgaSentence(m_config.ggaLat, m_config.ggaLon, m_config.ggaHeight);
    if (sentence.isEmpty()) return;

    m_socket->write(sentence);
    m_socket->flush();

    // 每次上传都记录位置；默认上传间隔为 1 秒
    LOG_INFO("NTRIP GGA sent: lat=%.7f lon=%.7f h=%.1f (%s)",
             m_config.ggaLat, m_config.ggaLon, m_config.ggaHeight,
             m_config.ggaFromSerial ? "from receiver" : "manual");
}

// 合成一条 NMEA GGA（无差分、单点定位），只用于向 VRS Caster 提供概略位置
QByteArray NtripClientWorker::makeGgaSentence(double lat, double lon, double height)
{
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        return QByteArray();
    }

    const QTime utc = QDateTime::currentDateTimeUtc().time();
    const QString t = QString("%1%2%3.%4")
            .arg(utc.hour(), 2, 10, QChar('0'))
            .arg(utc.minute(), 2, 10, QChar('0'))
            .arg(utc.second(), 2, 10, QChar('0'))
            .arg(utc.msec() / 10, 2, 10, QChar('0'));

    const double aLat = qAbs(lat);
    const int latDeg = static_cast<int>(aLat);
    const QString latMin = QString("%1").arg((aLat - latDeg) * 60.0, 7, 'f', 4, QChar('0'));

    const double aLon = qAbs(lon);
    const int lonDeg = static_cast<int>(aLon);
    const QString lonMin = QString("%1").arg((aLon - lonDeg) * 60.0, 7, 'f', 4, QChar('0'));

    const QString body = QString("GPGGA,%1,%2%3,%4,%5%6,%7,1,12,1.0,%8,M,0.0,M,,")
            .arg(t)
            .arg(latDeg, 2, 10, QChar('0'))
            .arg(latMin)
            .arg(lat >= 0.0 ? "N" : "S")
            .arg(lonDeg, 3, 10, QChar('0'))
            .arg(lonMin)
            .arg(lon >= 0.0 ? "E" : "W")
            .arg(height, 0, 'f', 1);

    QByteArray sentence = ("$" + body).toLatin1();

    unsigned char checksum = 0;
    for (int i = 1; i < sentence.size(); ++i) {
        checksum ^= static_cast<unsigned char>(sentence.at(i));
    }
    sentence += '*';
    sentence += QByteArray::number(static_cast<int>(checksum), 16)
            .rightJustified(2, '0').toUpper();
    sentence += "\r\n";
    return sentence;
}

// ============================================================
// UdpServerWorker
// ============================================================

UdpServerWorker::UdpServerWorker(quint16 port, QObject *parent)
    : QObject(parent)
{
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpServerWorker::onReadyRead);
}

UdpServerWorker::~UdpServerWorker()
{
    stop();
}

bool UdpServerWorker::start(quint16 port)
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        LOG_WARN("UDP server already bound to port %d", m_socket->localPort());
        return true;
    }

    if (!m_socket->bind(QHostAddress::Any, port)) {
        QString err = m_socket->errorString();
        LOG_ERROR("Failed to bind UDP port %d: %s", port, err.toStdString().c_str());
        emit errorOccurred(err);
        return false;
    }

    LOG_INFO("UDP server listening on port %d", m_socket->localPort());
    return true;
}

void UdpServerWorker::stop()
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
        LOG_INFO("UDP server stopped");
    }
}

bool UdpServerWorker::isRunning() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

quint16 UdpServerWorker::port() const
{
    return m_socket->localPort();
}

void UdpServerWorker::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(data.data(), data.size(), &sender, &senderPort);
        emit dataReceived(data, sender.toString(), senderPort);
    }
}

// ============================================================
// UdpClientWorker
// ============================================================

UdpClientWorker::UdpClientWorker(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_socket = new QUdpSocket(this);
}

UdpClientWorker::~UdpClientWorker()
{
    stop();
}

bool UdpClientWorker::start()
{
    if (m_socket->state() == QAbstractSocket::BoundState) return true;
    if (!m_socket->bind()) {
        QString err = m_socket->errorString();
        LOG_ERROR("UDP client bind failed: %s", err.toStdString().c_str());
        return false;
    }
    LOG_INFO("UDP client started (target: %s:%d)",
             m_config.host.toStdString().c_str(), m_config.port);
    return true;
}

void UdpClientWorker::stop()
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
        LOG_INFO("UDP client stopped");
    }
}

bool UdpClientWorker::isRunning() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

bool UdpClientWorker::write(const QByteArray &data)
{
    return m_socket->writeDatagram(data, QHostAddress(m_config.host), m_config.port) == data.size();
}

void UdpClientWorker::updateConfig(const Config &config)
{
    m_config = config;
}
