/**
 * RtcmRecorder - 按 UTC 日期记录下发的差分数据
 *
 * 把经过 StreamManager 分发（下发到各输出）的数据原样写入
 * <目录>/<UTC yyyyMMdd>.rtcm3，跨 UTC 日自动切换到新文件。
 */

#ifndef RTCMRECORDER_H
#define RTCMRECORDER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QFile>
#include <QDate>

class RtcmRecorder : public QObject
{
    Q_OBJECT

public:
    static RtcmRecorder &instance();

    /** 打开/关闭记录。关闭时关闭当前文件。 */
    void setEnabled(bool enabled);
    bool isEnabled() const;

    /** 记录目录（不存在会自动创建）。 */
    void setDirectory(const QString &dir);
    QString directory() const;

    /** 当前正在写的文件（未开始记录时为空）。 */
    QString currentFilePath() const;

    /** 本次运行累计写入字节数。 */
    quint64 totalBytes() const;

    /** 写入一段数据（线程安全；未启用时直接忽略）。 */
    void write(const QByteArray &data);

    /** 关闭当前文件（下次 write 时重新按日期打开）。 */
    void close();

    /** 从 strsvr.ini 的 [set] recordena / recorddir 读取设置并应用。 */
    void applyFromConfig();
    /** 保存设置到 strsvr.ini。 */
    void saveToConfig();

    ~RtcmRecorder() override;

signals:
    void recordingChanged(bool enabled);
    void fileChanged(const QString &path);

private:
    explicit RtcmRecorder(QObject *parent = nullptr);
    RtcmRecorder(const RtcmRecorder &) = delete;
    RtcmRecorder &operator=(const RtcmRecorder &) = delete;

    bool openForDate(const QDate &utcDate);
    QString fileNameForDate(const QDate &utcDate) const;

    mutable QMutex m_mutex;
    bool m_enabled = false;
    QString m_dir;
    QFile m_file;
    QDate m_fileDate;
    quint64 m_totalBytes = 0;
};

#endif // RTCMRECORDER_H
