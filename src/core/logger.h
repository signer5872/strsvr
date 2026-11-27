/**
 * Logger - Thread-safe logging system for STRSVR
 *
 * Supports multiple log levels, file output, and Qt signal integration
 * for GUI log display.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QDateTime>
#include <QList>
#include <memory>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        Fatal = 4
    };

    static Logger &instance();

    void setLevel(LogLevel level);
    LogLevel level() const;

    void setLogFile(const QString &path);
    void enableConsoleOutput(bool enable);

    void log(LogLevel level, const QString &message);
    void log(LogLevel level, const char *format, ...);

    // Get recent log entries for GUI display
    QList<QPair<QString, LogLevel>> recentLogs(int maxCount = 500) const;

    void shutdown();

    ~Logger() override;

signals:
    void logMessage(const QString &message, int level);

private:
    explicit Logger(QObject *parent = nullptr);

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static std::unique_ptr<Logger> s_instance;

    mutable QMutex m_mutex;
    LogLevel m_level;
    QFile m_logFile;
    QTextStream m_stream;
    bool m_consoleEnabled;
    QList<QPair<QString, LogLevel>> m_recentLogs;
    static constexpr int MAX_RECENT_LOGS = 5000;

    QString levelToString(LogLevel level) const;
    QString formatMessage(LogLevel level, const QString &msg) const;
};

// Convenience macros
#define LOG_DEBUG(fmt, ...) Logger::instance().log(Logger::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::instance().log(Logger::Info, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::instance().log(Logger::Warn, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::instance().log(Logger::Error, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) Logger::instance().log(Logger::Fatal, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
