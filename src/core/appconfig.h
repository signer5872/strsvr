/**
 * AppConfig - INI configuration file parser for STRSVR
 *
 * Handles all configuration from strsvr.ini matching the original
 * Delphi implementation's structure exactly.
 */

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <memory>
#include <QMap>
#include <QMutex>

class AppConfig : public QObject
{
    Q_OBJECT

public:
    static AppConfig &instance();

    bool load(const QString &path);
    bool save();

    QString filePath() const;

    // Generic getters
    QString getString(const QString &section, const QString &key, const QString &defaultValue = "") const;
    int getInt(const QString &section, const QString &key, int defaultValue = 0) const;
    double getDouble(const QString &section, const QString &key, double defaultValue = 0.0) const;
    bool getBool(const QString &section, const QString &key, bool defaultValue = false) const;

    // Generic setters
    void setString(const QString &section, const QString &key, const QString &value);
    void setInt(const QString &section, const QString &key, int value);
    void setDouble(const QString &section, const QString &key, double value);
    void setBool(const QString &section, const QString &key, bool value);

    // STRSVR-specific typed accessors for [set] section
    struct GlobalSettings {
        int input;          // Input stream selector
        int output1;        // Output 1 selector
        int output2;        // Output 2 selector
        int output3;        // Output 3 selector
        int tracelevel;     // Log/trace level
        int nmeareq;        // NMEA request mode
        int fswapmargin;    // Frequency swap margin (Hz)
        int relayback;      // Relay back mode
        int progbarrange;   // Progress bar range
        int staid;          // Station ID
        int stasel;         // Station selection
        QString anttype;    // Antenna type
        QString rcvtype;    // Receiver type

        // Server options [set] svropt_0 to svropt_5
        int svropt[6];

        // Antenna positions/offsets for 3 antennas
        int antpos[3];
        int antoff[3];
    };

    GlobalSettings &globalSettings();
    const GlobalSettings &globalSettings() const;

    // Conversion settings [conv]
    struct ConversionConfig {
        bool enabled;
        int inputChannel;
        int outputChannel;
        QString message;
        QString options;
    };

    // Serial command settings [serial]
    struct SerialCmdConfig {
        bool enabled;
        QString command;
    };

    // TCP/IP command settings [tcpip]
    struct TcpIpCmdConfig {
        bool enabled;
        QString command;
    };

    ~AppConfig() override;

signals:
    void configChanged();

private:
    explicit AppConfig(QObject *parent = nullptr);

    AppConfig(const AppConfig &) = delete;
    AppConfig &operator=(const AppConfig &) = delete;

    static std::unique_ptr<AppConfig> s_instance;

    mutable QMutex m_mutex;
    QString m_filePath;
    std::unique_ptr<QSettings> m_settings;
    GlobalSettings m_globalSettings;
    bool m_dirty;

    void loadGlobalSettings();
    void saveGlobalSettings();
};

#endif // APPCONFIG_H
