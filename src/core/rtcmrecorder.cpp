#include "rtcmrecorder.h"
#include "appconfig.h"
#include "logger.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>

RtcmRecorder &RtcmRecorder::instance()
{
    static RtcmRecorder s_instance;
    return s_instance;
}

RtcmRecorder::RtcmRecorder(QObject *parent)
    : QObject(parent)
    , m_dir(QDir::currentPath())
{
}

RtcmRecorder::~RtcmRecorder()
{
    close();
}

void RtcmRecorder::setEnabled(bool enabled)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_enabled == enabled) return;
        m_enabled = enabled;
        changed = true;
        if (!enabled && m_file.isOpen()) {
            m_file.flush();
            m_file.close();
            LOG_INFO("RTCM recording stopped: %s",
                     m_file.fileName().toStdString().c_str());
        }
        if (enabled) {
            LOG_INFO("RTCM recording enabled, dir=%s",
                     m_dir.toStdString().c_str());
        }
    }
    if (changed) emit recordingChanged(enabled);
}

bool RtcmRecorder::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

void RtcmRecorder::setDirectory(const QString &dir)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_dir == dir) return;
        m_dir = dir;
        if (m_file.isOpen()) {
            m_file.flush();
            m_file.close();
        }
    }
    LOG_INFO("RTCM record directory: %s", dir.toStdString().c_str());
}

QString RtcmRecorder::directory() const
{
    QMutexLocker locker(&m_mutex);
    return m_dir;
}

QString RtcmRecorder::currentFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.isOpen() ? m_file.fileName() : QString();
}

quint64 RtcmRecorder::totalBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalBytes;
}

QString RtcmRecorder::fileNameForDate(const QDate &utcDate) const
{
    return QDir(m_dir).filePath(utcDate.toString("yyyyMMdd") + ".rtcm3");
}

bool RtcmRecorder::openForDate(const QDate &utcDate)
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    QDir dir(m_dir);
    if (!dir.exists() && !dir.mkpath(".")) {
        LOG_ERROR("Cannot create record directory: %s",
                  m_dir.toStdString().c_str());
        return false;
    }

    m_file.setFileName(fileNameForDate(utcDate));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        LOG_ERROR("Cannot open record file %s: %s",
                  m_file.fileName().toStdString().c_str(),
                  m_file.errorString().toStdString().c_str());
        return false;
    }

    m_fileDate = utcDate;
    LOG_INFO("RTCM recording to %s", m_file.fileName().toStdString().c_str());
    return true;
}

void RtcmRecorder::write(const QByteArray &data)
{
    if (data.isEmpty()) return;

    QString openedPath;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled) return;

        const QDate utcToday = QDateTime::currentDateTimeUtc().date();
        if (!m_file.isOpen() || m_fileDate != utcToday) {
            if (!openForDate(utcToday)) return;
            openedPath = m_file.fileName();
        }

        if (m_file.write(data) != data.size()) {
            LOG_WARN("RTCM record write incomplete: %s",
                     m_file.errorString().toStdString().c_str());
            return;
        }
        m_file.flush();
        m_totalBytes += static_cast<quint64>(data.size());
    }

    if (!openedPath.isEmpty()) {
        emit fileChanged(openedPath);
    }
}

void RtcmRecorder::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

void RtcmRecorder::applyFromConfig()
{
    AppConfig &config = AppConfig::instance();
    QString dir = config.getString("set", "recorddir", QDir::currentPath());
    if (dir.trimmed().isEmpty()) dir = QDir::currentPath();
    bool enabled = config.getBool("set", "recordena", false);

    setDirectory(dir);
    setEnabled(enabled);
}

void RtcmRecorder::saveToConfig()
{
    AppConfig &config = AppConfig::instance();
    config.setString("set", "recorddir", directory());
    config.setBool("set", "recordena", isEnabled());
}
