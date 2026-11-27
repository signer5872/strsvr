#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <cstdarg>
#include <cstdio>

std::unique_ptr<Logger> Logger::s_instance;

Logger &Logger::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<Logger>(new Logger());
    }
    return *s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_level(Info)
    , m_consoleEnabled(true)
{
}

Logger::~Logger()
{
    shutdown();
}

void Logger::setLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_level = level;
}

Logger::LogLevel Logger::level() const
{
    QMutexLocker locker(&m_mutex);
    return m_level;
}

void Logger::setLogFile(const QString &path)
{
    QMutexLocker locker(&m_mutex);

    if (m_logFile.isOpen()) {
        m_stream.flush();
        m_logFile.close();
    }

    // Ensure directory exists
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    m_logFile.setFileName(path);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream.setDevice(&m_logFile);
        m_stream << "\n\n===== STRSVR Session Started at "
                 << QDateTime::currentDateTime().toString(Qt::ISODate)
                 << " =====\n\n";
        m_stream.flush();
    }
}

void Logger::enableConsoleOutput(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_consoleEnabled = enable;
}

void Logger::log(LogLevel level, const QString &message)
{
    if (level < m_level) return;

    QString formatted = formatMessage(level, message);

    {
        QMutexLocker locker(&m_mutex);

        // Store in recent logs for GUI
        m_recentLogs.append(qMakePair(formatted, level));
        if (m_recentLogs.size() > MAX_RECENT_LOGS) {
            m_recentLogs.removeFirst();
        }

        // Write to file
        if (m_logFile.isOpen()) {
            m_stream << formatted << "\n";
            m_stream.flush();
        }

        // Console output
        if (m_consoleEnabled) {
            QTextStream console(stderr);
            console << formatted << "\n";
            console.flush();
        }
    }

    emit logMessage(formatted, static_cast<int>(level));
}

void Logger::log(LogLevel level, const char *format, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(level, QString::fromLocal8Bit(buffer));
}

QList<QPair<QString, Logger::LogLevel>> Logger::recentLogs(int maxCount) const
{
    QMutexLocker locker(&m_mutex);
    if (maxCount >= m_recentLogs.size()) {
        return m_recentLogs;
    }
    return m_recentLogs.mid(m_recentLogs.size() - maxCount);
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_stream << "\n===== STRSVR Session Ended at "
                 << QDateTime::currentDateTime().toString(Qt::ISODate)
                 << " =====\n\n";
        m_stream.flush();
        m_logFile.close();
    }
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level) {
    case Debug: return "DEBUG";
    case Info:  return "INFO";
    case Warn:  return "WARN";
    case Error: return "ERROR";
    case Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

QString Logger::formatMessage(LogLevel level, const QString &msg) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString lvl = levelToString(level);
    return QString("[%1] [%2] %3").arg(timestamp, lvl, msg);
}
