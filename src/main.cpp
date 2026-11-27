/**
 * STRSVR - GNSS Stream Server (Linux Port)
 *
 * Original Author: T.Takasu (Copyright 2007-2017)
 * This is a C++/Qt5 reimplementation for Linux.
 *
 * A server application that:
 *  - Receives GNSS data from serial ports, TCP/UDP, files, NTRIP
 *  - Parses NMEA sentences (GGA, GSA, GSV, RMC, ZDA) and CMR protocol
 *  - Converts and distributes data to multiple output channels
 *  - Provides GNSS data relay capabilities
 */

#include <QApplication>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QIcon>
#include <QFont>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QLocale>

#include "gui/mainwindow.h"
#include "core/logger.h"
#include "core/appconfig.h"
#include "core/language.h"
#include "core/rtcmrecorder.h"

int main(int argc, char *argv[])
{
    // Enable high DPI support
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // Application metadata
    app.setApplicationName("STRSVR");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("STRSVR");
    app.setOrganizationDomain("local");

    // Command-line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "STRSVR - GNSS Stream Server\n\n"
        "A data stream server for GNSS applications.\n"
        "Supports NMEA/CMR protocols, serial/TCP/UDP/NTRIP communications."
    );
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Specify configuration file path.",
        "config",
        "strsvr.ini"
    );
    parser.addOption(configOption);

    QCommandLineOption noGuiOption(
        QStringList() << "nogui",
        "Run in headless mode (no GUI)."
    );
    parser.addOption(noGuiOption);

    parser.process(app);

    // Initialize logger
    Logger &logger = Logger::instance();
    QString logFile = QDir::currentPath() + "/strsvr.log";
    logger.setLogFile(logFile);
    logger.setLevel(Logger::Info);

    LOG_INFO("============================================================");
    LOG_INFO("STRSVR GNSS Stream Server v%s", app.applicationVersion().toStdString().c_str());
    LOG_INFO("============================================================");

    // Load configuration
    QString configPath = parser.value(configOption);
    AppConfig &config = AppConfig::instance();
    if (!config.load(configPath)) {
        LOG_WARN("Failed to load config file: %s (using defaults)", configPath.toStdString().c_str());
    } else {
        LOG_INFO("Configuration loaded from: %s", configPath.toStdString().c_str());
    }

    // Apply trace level from config
    // 原版 RTKLIB tracelevel 语义：0=关, 1=INFO, 2=DEBUG (越大越详细)
    // 内部 Logger 枚举：Debug=0, Info=1, Warn=2, Error=3, Fatal=4
    // 过滤条件 if (level < m_level) return; —— 数字越大越严格
    // 所以把 ini 数值反向映射成内部枚举值：
    //   ini=0 → Error(3)  基本关闭
    //   ini=1 → Info(1)   正常运行级别（Info 及以上）
    //   ini=2 → Debug(0)  全部打印（最详细）
    const int iniTraceLevel = config.getInt("set", "tracelevel", 1);
    Logger::LogLevel lvl;
    switch (iniTraceLevel) {
    case 0:  lvl = Logger::Error;   break;
    case 1:  lvl = Logger::Info;    break;
    case 2:
    default: lvl = Logger::Debug;   break;
    }
    logger.setLevel(lvl);

    // Apply UI language from config (en / zh)
    LanguageManager::instance().applyFromConfig();

    // Apply RTCM record settings ([set] recordena / recorddir)
    RtcmRecorder::instance().applyFromConfig();

    // Set application style (固定 Fusion，避免不同发行版默认主题差异)
    app.setStyle(QStyleFactory::create("Fusion"));

    // 统一中文字体，减少 16.04/18.04 自带字体造成的字形与行距差异
    {
        const QStringList families = QFontDatabase().families();
        const char *preferred[] = {
            "Noto Sans CJK SC", "Noto Sans SC", "Source Han Sans CN",
            "WenQuanYi Micro Hei", "WenQuanYi Zen Hei", "Droid Sans Fallback"
        };
        QFont f = app.font();
        for (const char *name : preferred) {
            const QString fam = QString::fromLatin1(name);
            if (families.contains(fam)) {
                f.setFamily(fam);
                break;
            }
        }
        app.setFont(f);
    }

    // Main window
    MainWindow window;
    window.show();

    LOG_INFO("Application started successfully");

    int result = app.exec();

    // Cleanup
    LOG_INFO("Application exiting with code %d", result);
    config.save();
    logger.shutdown();

    return result;
}
