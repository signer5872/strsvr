#include "language.h"
#include "appconfig.h"
#include "logger.h"

#include <QApplication>
#include <QTranslator>

// ======================================================================
// 自定义翻译器：查内置词典，查不到时返回空串（Qt 会回退到英文原文）
// ======================================================================
class LanguageManager::ZhTranslator : public QTranslator
{
public:
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (!sourceText || !*sourceText) {
            return QString();
        }
        return chineseDictionary().value(QString::fromUtf8(sourceText));
    }

    bool isEmpty() const override
    {
        return chineseDictionary().isEmpty();
    }
};

// ======================================================================
// 单例
// ======================================================================
LanguageManager &LanguageManager::instance()
{
    static LanguageManager inst;
    return inst;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent)
{
}

LanguageManager::~LanguageManager()
{
}

void LanguageManager::applyFromConfig()
{
    const QString code = AppConfig::instance().getString("set", "language", "en");
    m_lang = (code.compare("zh", Qt::CaseInsensitive) == 0) ? Chinese : English;
    install();
}

void LanguageManager::setLang(Language lang)
{
    if (m_lang == lang) {
        return;
    }
    m_lang = lang;
    AppConfig::instance().setString("set", "language", lang == Chinese ? "zh" : "en");
    install();
    emit languageChanged();

    LOG_INFO("UI language switched to %s", lang == Chinese ? "zh" : "en");
}

void LanguageManager::install()
{
    if (!m_translator) {
        m_translator = new ZhTranslator();
    }
    if (qApp) {
        qApp->removeTranslator(m_translator);
        if (m_lang == Chinese) {
            qApp->installTranslator(m_translator);
        }
    }
}

QString LanguageManager::text(const QString &english) const
{
    if (m_lang != Chinese) {
        return english;
    }
    return chineseDictionary().value(english, english);
}

// ======================================================================
// 内置词典（键 = 代码中 tr() 的英文原文）
// ======================================================================
const QHash<QString, QString> &LanguageManager::chineseDictionary()
{
    static const QHash<QString, QString> dict = {
        // ---- 主窗口 / 标题 ----
        { "STRSVR - GNSS Stream Server", QString::fromUtf8("STRSVR - GNSS 数据流服务器") },
        { "Confirm Exit", QString::fromUtf8("确认退出") },
        { "STRSVR is currently running.\nAre you sure you want to exit?",
          QString::fromUtf8("STRSVR 正在运行。\n确定要退出吗？") },

        // ---- 菜单 ----
        { "&File", QString::fromUtf8("文件(&F)") },
        { "&Preferences...", QString::fromUtf8("首选项(&P)...") },
        { "E&xit", QString::fromUtf8("退出(&X)") },
        { "&Stream", QString::fromUtf8("数据流(&S)") },
        { "&Start Server", QString::fromUtf8("启动服务器(&S)") },
        { "S&top Server", QString::fromUtf8("停止服务器(&T)") },
        { "&Restart Server", QString::fromUtf8("重启服务器(&R)") },
        { "Serial Port &Settings...", QString::fromUtf8("串口设置(&S)...") },
        { "&Network Settings...", QString::fromUtf8("网络设置(&N)...") },
        { "&Channel", QString::fromUtf8("通道(&C)") },
        { "Add &Serial Channel", QString::fromUtf8("添加串口通道(&S)") },
        { "Add &TCP Server Channel", QString::fromUtf8("添加 TCP 服务端通道(&T)") },
        { "Add TC&P Client Channel", QString::fromUtf8("添加 TCP 客户端通道(&P)") },
        { "Add UD&P Server Channel", QString::fromUtf8("添加 UDP 服务端通道(&P)") },
        { "Add &UDP Client Channel", QString::fromUtf8("添加 UDP 客户端通道(&U)") },
        { "&Enable Selected", QString::fromUtf8("启用所选(&E)") },
        { "&Disable Selected", QString::fromUtf8("禁用所选(&D)") },
        { "&Configure Selected...", QString::fromUtf8("配置所选(&C)...") },
        { "&Remove Selected", QString::fromUtf8("移除所选(&R)") },
        { "&View", QString::fromUtf8("视图(&V)") },
        { "&Help", QString::fromUtf8("帮助(&H)") },
        { "&About STRSVR", QString::fromUtf8("关于 STRSVR(&A)") },
        { "About &Qt", QString::fromUtf8("关于 Qt(&Q)") },

        // ---- 工具栏 / 按钮 ----
        { "Main Toolbar", QString::fromUtf8("主工具栏") },
        { "Start", QString::fromUtf8("启动") },
        { "Stop", QString::fromUtf8("停止") },
        { "Restart", QString::fromUtf8("重启") },
        { "Start all enabled channels", QString::fromUtf8("启动所有已启用的通道") },
        { "Stop all channels", QString::fromUtf8("停止所有通道") },
        { "Add Serial", QString::fromUtf8("添加串口") },
        { "Add a new serial port channel", QString::fromUtf8("新增一个串口通道") },
        { "Add TCP Server", QString::fromUtf8("添加 TCP 服务端") },
        { "Add a new TCP server channel", QString::fromUtf8("新增一个 TCP 服务端通道") },
        { "Add TCP Client", QString::fromUtf8("添加 TCP 客户端") },
        { "Add a new TCP client channel", QString::fromUtf8("新增一个 TCP 客户端通道") },
        { "Add UDP Server", QString::fromUtf8("添加 UDP 服务端") },
        { "Add UDP Client", QString::fromUtf8("添加 UDP 客户端") },
        { "Configure...", QString::fromUtf8("配置...") },
        { "Enable", QString::fromUtf8("启用") },
        { "Disable", QString::fromUtf8("禁用") },
        { "Remove", QString::fromUtf8("移除") },
        { "Clear", QString::fromUtf8("清空") },
        { "Auto-scroll", QString::fromUtf8("自动滚动") },
        { "Close", QString::fromUtf8("关闭") },

        // ---- 标签页 ----
        { "Dashboard", QString::fromUtf8("概览") },
        { "Channels", QString::fromUtf8("通道") },
        { "Stream Monitor", QString::fromUtf8("数据流监视") },
        { "Log", QString::fromUtf8("日志") },

        // ---- 概览面板 ----
        { "Server Control", QString::fromUtf8("服务器控制") },
        { "Status: Stopped", QString::fromUtf8("状态：已停止") },
        { "Status: Running", QString::fromUtf8("状态：运行中") },
        { "GNSS Status", QString::fromUtf8("GNSS 状态") },
        { "System", QString::fromUtf8("系统") },
        { "Fix: --", QString::fromUtf8("定位：--") },
        { "Fix: No fix", QString::fromUtf8("定位：无信号") },
        { "Fix: %1", QString::fromUtf8("定位：%1") },
        { "Position: No fix", QString::fromUtf8("位置：无定位") },
        { "Position: Waiting for data...", QString::fromUtf8("位置：等待数据...") },
        { "Lat: %1\nLon: %2", QString::fromUtf8("纬度：%1\n经度：%2") },
        { "Satellites: --", QString::fromUtf8("卫星数：--") },
        { "Satellites: %1", QString::fromUtf8("卫星数：%1") },
        { "Altitude: -- m", QString::fromUtf8("海拔：-- 米") },
        { "Altitude: %1 m", QString::fromUtf8("海拔：%1 米") },
        { "Last fix: --", QString::fromUtf8("最近定位：--") },
        { "Last fix: %1", QString::fromUtf8("最近定位：%1") },
        { "STRSVR Version: 1.0.0 (Linux)", QString::fromUtf8("STRSVR 版本：1.0.0 (Linux)") },
        { "Original by T.Takasu (2007-2017)", QString::fromUtf8("原作者：T.Takasu (2007-2017)") },
        { "No position fix", QString::fromUtf8("无定位") },
        { "Channels: 0", QString::fromUtf8("通道数：0") },
        { "Channels: %1", QString::fromUtf8("通道数：%1") },

        // ---- 通道表格 ----
        { "ID", QString::fromUtf8("ID") },
        { "Name", QString::fromUtf8("名称") },
        { "Type", QString::fromUtf8("类型") },
        { "Enabled", QString::fromUtf8("已启用") },
        { "Bytes In", QString::fromUtf8("接收字节") },
        { "Bytes Out", QString::fromUtf8("发送字节") },
        { "Last Activity", QString::fromUtf8("最后活动") },
        { "Yes", QString::fromUtf8("是") },
        { "No", QString::fromUtf8("否") },
        { "Never", QString::fromUtf8("从未") },
        { "Serial", QString::fromUtf8("串口") },
        { "TCP Server", QString::fromUtf8("TCP 服务端") },
        { "TCP Client", QString::fromUtf8("TCP 客户端") },
        { "UDP Server", QString::fromUtf8("UDP 服务端") },
        { "UDP Client", QString::fromUtf8("UDP 客户端") },
        { "Unknown", QString::fromUtf8("未知") },
        { "Serial-%1", QString::fromUtf8("串口-%1") },
        { "TCP-Server-%1", QString::fromUtf8("TCP服务端-%1") },
        { "TCP-Client-%1", QString::fromUtf8("TCP客户端-%1") },
        { "UDP-Server-%1", QString::fromUtf8("UDP服务端-%1") },
        { "UDP-Client-%1", QString::fromUtf8("UDP客户端-%1") },
        { "Select Channel", QString::fromUtf8("选择通道") },
        { "Please select a channel first.", QString::fromUtf8("请先选择一个通道。") },
        { "Configure Serial Channel #%1", QString::fromUtf8("配置串口通道 #%1") },
        { "Configure Network Channel #%1", QString::fromUtf8("配置网络通道 #%1") },
        { "Stream monitor - will display raw NMEA data as it flows...",
          QString::fromUtf8("数据流监视 - 实时显示原始 NMEA 数据...") },

        // ---- 托盘 ----
        { "Show", QString::fromUtf8("显示") },
        { "Quit", QString::fromUtf8("退出") },

        // ---- 首选项对话框 ----
        { "Preferences", QString::fromUtf8("首选项") },
        { "Global", QString::fromUtf8("全局") },
        { "Log Level:", QString::fromUtf8("日志级别：") },
        { "0 - Verbose", QString::fromUtf8("0 - 详细") },
        { "1 - Debug", QString::fromUtf8("1 - 调试") },
        { "2 - Info", QString::fromUtf8("2 - 信息") },
        { "3 - Warning", QString::fromUtf8("3 - 警告") },
        { "4 - Error", QString::fromUtf8("4 - 错误") },
        { "NMEA Request:", QString::fromUtf8("NMEA 请求：") },
        { "None", QString::fromUtf8("无") },
        { "GGA Only", QString::fromUtf8("仅 GGA") },
        { "GGA + GSA + RMC + GSV", QString::fromUtf8("GGA + GSA + RMC + GSV") },
        { "Frequency Swap Margin:", QString::fromUtf8("频率切换余量：") },
        { "Relay Back Mode:", QString::fromUtf8("回传模式：") },
        { "Station ID:", QString::fromUtf8("站点 ID：") },
        { "Station Selection:", QString::fromUtf8("站点选择：") },
        { "Antenna Type:", QString::fromUtf8("天线类型：") },
        { "Receiver Type:", QString::fromUtf8("接收机类型：") },
        { "Server Options", QString::fromUtf8("服务器选项") },
        { "Server Option 0", QString::fromUtf8("服务器选项 0") },
        { "Server Option 1", QString::fromUtf8("服务器选项 1") },
        { "Server Option 2", QString::fromUtf8("服务器选项 2") },
        { "Server Option 3 (DGPS buffer)", QString::fromUtf8("服务器选项 3 (DGPS 缓存)") },
        { "Server Option 4", QString::fromUtf8("服务器选项 4") },
        { "Server Option 5", QString::fromUtf8("服务器选项 5") },
        { "Paths", QString::fromUtf8("路径") },
        { "Log File:", QString::fromUtf8("日志文件：") },
        { "Source Table File:", QString::fromUtf8("源列表文件：") },
        { "Station Position File:", QString::fromUtf8("站点坐标文件：") },
        { "Proxy Address:", QString::fromUtf8("代理地址：") },
        { "http://proxy:port", QString::fromUtf8("http://代理:端口") },

        // ---- 串口设置对话框 ----
        { "Serial Port Settings", QString::fromUtf8("串口设置") },
        { "Port", QString::fromUtf8("端口") },
        { "Port:", QString::fromUtf8("端口：") },
        { "Refresh", QString::fromUtf8("刷新") },
        { "Parameters", QString::fromUtf8("参数") },
        { "Baud Rate:", QString::fromUtf8("波特率：") },
        { "Data Bits:", QString::fromUtf8("数据位：") },
        { "Parity:", QString::fromUtf8("校验位：") },
        { "Stop Bits:", QString::fromUtf8("停止位：") },
        { "Flow Control:", QString::fromUtf8("流控制：") },
        { "5 bits", QString::fromUtf8("5 位") },
        { "6 bits", QString::fromUtf8("6 位") },
        { "7 bits", QString::fromUtf8("7 位") },
        { "8 bits", QString::fromUtf8("8 位") },
        { "Even", QString::fromUtf8("偶校验") },
        { "Odd", QString::fromUtf8("奇校验") },
        { "Mark", QString::fromUtf8("标记") },
        { "Space", QString::fromUtf8("空格") },
        { "1 bit", QString::fromUtf8("1 位") },
        { "1.5 bits", QString::fromUtf8("1.5 位") },
        { "2 bits", QString::fromUtf8("2 位") },
        { "RTS/CTS", QString::fromUtf8("RTS/CTS") },
        { "XON/XOFF", QString::fromUtf8("XON/XOFF") },
        { "Startup Commands", QString::fromUtf8("启动命令") },
        { "Comma-separated commands, e.g. $PCAS03,1,0,0,1,0*15",
          QString::fromUtf8("逗号分隔的命令，例如 $PCAS03,1,0,0,1,0*15") },
        { "Commands will be sent when the port opens.\nUse \\r and \\n for line endings.",
          QString::fromUtf8("端口打开时会发送这些命令。\n使用 \\r 和 \\n 表示换行。") },

        // ---- 网络设置对话框 ----
        { "Network Settings", QString::fromUtf8("网络设置") },
        { "Listen Port:", QString::fromUtf8("监听端口：") },
        { "TCP Server accepts multiple clients and broadcasts data to all connected clients.",
          QString::fromUtf8("TCP 服务端接受多个客户端，并把数据广播给所有已连接客户端。") },
        { "Host/IP:", QString::fromUtf8("主机/IP：") },
        { "Port:", QString::fromUtf8("端口：") },
        { "Auto-reconnect on disconnect", QString::fromUtf8("断开后自动重连") },
        { "Reconnect Interval:", QString::fromUtf8("重连间隔：") },
        { "e.g., 192.168.1.100", QString::fromUtf8("例如 192.168.1.100") },
        { "Target Host:", QString::fromUtf8("目标主机：") },
        { "Target Port:", QString::fromUtf8("目标端口：") },
        { "e.g., 224.0.0.1 (multicast)", QString::fromUtf8("例如 224.0.0.1（组播）") },
        { "NTRIP Client", QString::fromUtf8("NTRIP 客户端") },
        { "Caster Host:", QString::fromUtf8("Caster 主机：") },
        { "Caster Port:", QString::fromUtf8("Caster 端口：") },
        { "Mountpoint:", QString::fromUtf8("挂载点：") },
        { "Username:", QString::fromUtf8("用户名：") },
        { "Password:", QString::fromUtf8("密码：") },
        { "e.g., caster.igs.org", QString::fromUtf8("例如 caster.igs.org") },
        { "e.g., ALGO0", QString::fromUtf8("例如 ALGO0") },
        { "Username (optional)", QString::fromUtf8("用户名（可选）") },
        { "Password (optional)", QString::fromUtf8("密码（可选）") },
        { "Upload GGA to caster (VRS)", QString::fromUtf8("上传 GGA 到 Caster（VRS）") },
        { "Use GGA from input stream (auto)", QString::fromUtf8("使用输入数据中的 GGA（自动）") },
        { "Latitude (deg):", QString::fromUtf8("纬度（度）：") },
        { "Longitude (deg):", QString::fromUtf8("经度（度）：") },
        { "Height (m):", QString::fromUtf8("高程（米）：") },
        { "e.g., 39.9042000", QString::fromUtf8("例如 39.9042000") },
        { "e.g., 116.4074000", QString::fromUtf8("例如 116.4074000") },
        { "e.g., 50.0", QString::fromUtf8("例如 50.0") },

        // ---- 关于对话框 ----
        { "About STRSVR", QString::fromUtf8("关于 STRSVR") },
        { "STRSVR", QString::fromUtf8("STRSVR") },
        { "GNSS Stream Server - Linux Port v1.0.0",
          QString::fromUtf8("GNSS 数据流服务器 - Linux 版 v1.0.0") },
        { "Built with C++17, Qt 5.15+", QString::fromUtf8("使用 C++17 与 Qt 5 构建") },

        // ---- 主界面（固定布局，复刻原版） ----
        { "&Start", QString::fromUtf8("启动(&S)") },
        { "S&top", QString::fromUtf8("停止(&T)") },
        { "&Options...", QString::fromUtf8("选项(&O)...") },
        { "Stream", QString::fromUtf8("数据流") },
        { "Opt", QString::fromUtf8("选项") },
        { "Cmd", QString::fromUtf8("命令") },
        { "Conv", QString::fromUtf8("转换") },
        { "Bytes", QString::fromUtf8("字节") },
        { "Bps", QString::fromUtf8("Bps") },
        { "(0) Input", QString::fromUtf8("(0) 输入") },
        { "(%1) Output", QString::fromUtf8("(%1) 输出") },
        { "File", QString::fromUtf8("文件") },
        { "NTRIP Server", QString::fromUtf8("NTRIP 服务端") },
        { "NTRIP Caster", QString::fromUtf8("NTRIP Caster") },

        // ---- 数据记录 ----
        { "Record", QString::fromUtf8("数据记录") },
        { "Language:", QString::fromUtf8("界面语言：") },
        { "Record RTCM data", QString::fromUtf8("记录 RTCM 差分数据") },
        { "Enable RTCM data recording", QString::fromUtf8("启用 RTCM 差分数据记录") },
        { "Directory:", QString::fromUtf8("保存目录：") },
        { "Browse...", QString::fromUtf8("浏览...") },
        { "Current file: --", QString::fromUtf8("当前文件：--") },
        { "Current file: %1", QString::fromUtf8("当前文件：%1") },
        { "Select Record Directory", QString::fromUtf8("选择记录目录") },
        { "Data distributed to the outputs is saved as <UTC date>.rtcm3\n(e.g. 20261124.rtcm3). A new file is started when the UTC date changes.",
          QString::fromUtf8("下发给各输出的数据会保存为 <UTC 日期>.rtcm3\n（例如 20261124.rtcm3），跨 UTC 日期时自动切换新文件。") },
        { "STRSVR - REC %1", QString::fromUtf8("STRSVR - 记录 %1") },
        { "Conversion is not supported in this build",
          QString::fromUtf8("此版本不支持数据转换") },
        { "NTRIP Server/Caster output is not supported in this build.",
          QString::fromUtf8("此版本不支持 NTRIP 服务端/Caster 输出。") },
        { "Startup Command", QString::fromUtf8("启动命令") },
        { "Commands sent when the stream starts (one per line):",
          QString::fromUtf8("数据流启动时发送的命令（每行一条）：") },
        { "Select Output File", QString::fromUtf8("选择输出文件") },
        { "Select Input File", QString::fromUtf8("选择输入文件") },
        { "Select Log File", QString::fromUtf8("选择日志文件") },
        { "Please configure at least one output stream.",
          QString::fromUtf8("请至少配置一个输出数据流。") },
        { "%1: %2. Please click \"...\" to set the stream options.",
          QString::fromUtf8("%1：%2。请点击“...”设置该数据流参数。") },
        { "serial port is not set", QString::fromUtf8("未设置串口") },
        { "TCP client host is not set", QString::fromUtf8("未设置 TCP 客户端主机") },
        { "NTRIP caster host is not set", QString::fromUtf8("未设置 NTRIP Caster 主机") },
        { "file is not set", QString::fromUtf8("未设置文件") },

        // ---- NMEA 定位质量（由 LanguageManager::text() 转换） ----
        { "Invalid", QString::fromUtf8("无效") },
        { "GPS Fix", QString::fromUtf8("GPS 定位") },
        { "DGPS Fix", QString::fromUtf8("DGPS 定位") },
        { "PPS Fix", QString::fromUtf8("PPS 定位") },
        { "RTK Fix", QString::fromUtf8("RTK 固定解") },
        { "RTK Float", QString::fromUtf8("RTK 浮动解") },
        { "Estimated", QString::fromUtf8("推算") },
        { "Manual", QString::fromUtf8("手动") },
        { "Simulation", QString::fromUtf8("模拟") },
    };
    return dict;
}
