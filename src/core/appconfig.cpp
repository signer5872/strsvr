#include "appconfig.h"
#include "logger.h"

#include <QDir>
#include <QFile>

std::unique_ptr<AppConfig> AppConfig::s_instance;

AppConfig &AppConfig::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<AppConfig>(new AppConfig());
    }
    return *s_instance;
}

AppConfig::AppConfig(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(QSettings::IniFormat, QSettings::UserScope, "STRSVR", "strsvr"))
    , m_dirty(false)
{
}

AppConfig::~AppConfig()
{
    if (m_dirty) {
        save();
    }
}

bool AppConfig::load(const QString &path)
{
    QMutexLocker locker(&m_mutex);

    QFileInfo fi(path);
    QString resolvedPath;
    if (fi.isAbsolute()) {
        resolvedPath = path;
    } else {
        resolvedPath = QDir::currentPath() + "/" + path;
    }

    if (!QFile::exists(resolvedPath)) {
        LOG_WARN("Config file not found: %s", resolvedPath.toStdString().c_str());
        // Still create settings with default values
        m_settings.reset(new QSettings(resolvedPath, QSettings::IniFormat));
        m_filePath = resolvedPath;
        loadGlobalSettings();
        return false;
    }

    m_settings.reset(new QSettings(resolvedPath, QSettings::IniFormat));
    m_filePath = resolvedPath;
    loadGlobalSettings();

    LOG_INFO("Config loaded from: %s", resolvedPath.toStdString().c_str());
    m_dirty = false;
    return true;
}

bool AppConfig::save()
{
    QMutexLocker locker(&m_mutex);
    if (m_filePath.isEmpty()) {
        LOG_WARN("No config file path set, cannot save");
        return false;
    }

    saveGlobalSettings();
    m_settings->sync();
    m_dirty = false;

    LOG_INFO("Config saved to: %s", m_filePath.toStdString().c_str());
    return true;
}

QString AppConfig::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

// ===== Generic getters =====

QString AppConfig::getString(const QString &section, const QString &key, const QString &defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    QString val = m_settings->value(key, defaultValue).toString();
    m_settings->endGroup();
    return val;
}

int AppConfig::getInt(const QString &section, const QString &key, int defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    int val = m_settings->value(key, defaultValue).toInt();
    m_settings->endGroup();
    return val;
}

double AppConfig::getDouble(const QString &section, const QString &key, double defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    double val = m_settings->value(key, defaultValue).toDouble();
    m_settings->endGroup();
    return val;
}

bool AppConfig::getBool(const QString &section, const QString &key, bool defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    bool val = m_settings->value(key, defaultValue).toBool();
    m_settings->endGroup();
    return val;
}

// ===== Generic setters =====

void AppConfig::setString(const QString &section, const QString &key, const QString &value)
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    m_settings->setValue(key, value);
    m_settings->endGroup();
    m_dirty = true;
    locker.unlock();
    emit configChanged();
}

void AppConfig::setInt(const QString &section, const QString &key, int value)
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    m_settings->setValue(key, value);
    m_settings->endGroup();
    m_dirty = true;
    locker.unlock();
    emit configChanged();
}

void AppConfig::setDouble(const QString &section, const QString &key, double value)
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    m_settings->setValue(key, value);
    m_settings->endGroup();
    m_dirty = true;
    locker.unlock();
    emit configChanged();
}

void AppConfig::setBool(const QString &section, const QString &key, bool value)
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(section);
    m_settings->setValue(key, value ? 1 : 0);
    m_settings->endGroup();
    m_dirty = true;
    locker.unlock();
    emit configChanged();
}

// ===== Global Settings =====

AppConfig::GlobalSettings &AppConfig::globalSettings()
{
    return m_globalSettings;
}

const AppConfig::GlobalSettings &AppConfig::globalSettings() const
{
    return m_globalSettings;
}

void AppConfig::loadGlobalSettings()
{
    m_settings->beginGroup("set");
    m_globalSettings.input = m_settings->value("input", 0).toInt();
    m_globalSettings.output1 = m_settings->value("output1", 0).toInt();
    m_globalSettings.output2 = m_settings->value("output2", 0).toInt();
    m_globalSettings.output3 = m_settings->value("output3", 0).toInt();
    m_globalSettings.tracelevel = m_settings->value("tracelevel", 0).toInt();
    m_globalSettings.nmeareq = m_settings->value("nmeareq", 0).toInt();
    m_globalSettings.fswapmargin = m_settings->value("fswapmargin", 30).toInt();
    m_globalSettings.relayback = m_settings->value("relayback", 0).toInt();
    m_globalSettings.progbarrange = m_settings->value("progbarrange", 2000).toInt();
    m_globalSettings.staid = m_settings->value("staid", 0).toInt();
    m_globalSettings.stasel = m_settings->value("stasel", 0).toInt();
    m_globalSettings.anttype = m_settings->value("anttype", "").toString();
    m_globalSettings.rcvtype = m_settings->value("rcvtype", "").toString();

    for (int i = 0; i < 6; ++i) {
        QString key = QString("svropt_%1").arg(i);
        m_globalSettings.svropt[i] = m_settings->value(key, (i == 3) ? 32768 : (i == 2) ? 1000 : 10000).toInt();
    }

    for (int i = 0; i < 3; ++i) {
        m_globalSettings.antpos[i] = m_settings->value(QString("antpos_%1").arg(i), 0).toInt();
        m_globalSettings.antoff[i] = m_settings->value(QString("antoff_%1").arg(i), 0).toInt();
    }
    m_settings->endGroup();
}

void AppConfig::saveGlobalSettings()
{
    m_settings->beginGroup("set");
    m_settings->setValue("input", m_globalSettings.input);
    m_settings->setValue("output1", m_globalSettings.output1);
    m_settings->setValue("output2", m_globalSettings.output2);
    m_settings->setValue("output3", m_globalSettings.output3);
    m_settings->setValue("tracelevel", m_globalSettings.tracelevel);
    m_settings->setValue("nmeareq", m_globalSettings.nmeareq);
    m_settings->setValue("fswapmargin", m_globalSettings.fswapmargin);
    m_settings->setValue("relayback", m_globalSettings.relayback);
    m_settings->setValue("progbarrange", m_globalSettings.progbarrange);
    m_settings->setValue("staid", m_globalSettings.staid);
    m_settings->setValue("stasel", m_globalSettings.stasel);
    m_settings->setValue("anttype", m_globalSettings.anttype);
    m_settings->setValue("rcvtype", m_globalSettings.rcvtype);

    for (int i = 0; i < 6; ++i) {
        m_settings->setValue(QString("svropt_%1").arg(i), m_globalSettings.svropt[i]);
    }
    for (int i = 0; i < 3; ++i) {
        m_settings->setValue(QString("antpos_%1").arg(i), m_globalSettings.antpos[i]);
        m_settings->setValue(QString("antoff_%1").arg(i), m_globalSettings.antoff[i]);
    }
    m_settings->endGroup();
}
