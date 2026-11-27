/**
 * MainWindow - STRSVR 主界面
 *
 * 按原版 strsvr.exe（RTKLIB STRSVR 2.4.3）固定布局复刻：
 *   一行输入 + 三行输出，底部 Start/Stop/Options/Exit。
 * 新增：把下发的差分数据按 UTC 日期记录为 <yyyyMMdd>.rtcm3。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <QColor>
#include <QPoint>

#include "core/streammanager.h"
#include "io/serialport.h"
#include "io/network.h"

class QLabel;
class QComboBox;
class QPushButton;
class QAction;
class QMenu;
class QCheckBox;
class QLineEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onStart();
    void onStop();
    void onOptions();
    void onToggleLanguage();
    void onToggleRecord();
    void onRecordToggled(bool enabled);
    void onRecordDirBrowse();
    void onRefreshStats();
    void onChannelStarted(int channelId);
    void onChannelStopped(int channelId);
    void onChannelError(int channelId, const QString &error);

private:
    struct StreamRow {
        int index = 0;                 // 0=输入, 1..3=输出
        ChannelType type = ChannelType::None;
        int typeIndex = 0;             // 下拉框当前项
        int channelId = -1;

        quint64 lastBytes = 0;
        quint64 lastBps = 0;
        QElapsedTimer bpsTimer;

        SerialPortWorker::Config serial;
        TcpClientWorker::Config tcpClient;
        quint16 tcpServerPort = 2101;
        quint16 udpServerPort = 2101;
        UdpClientWorker::Config udpClient;
        NtripClientWorker::Config ntrip;
        QString filePath;
        QString command;

        QWidget *panel = nullptr;
        QLabel *label = nullptr;
        QComboBox *typeCombo = nullptr;
        QPushButton *btnOpt = nullptr;
        QPushButton *btnCmd = nullptr;
        QPushButton *btnConv = nullptr;
        QPushButton *btnLog = nullptr;
        QLabel *ind = nullptr;
        QLabel *indLog = nullptr;
        QLabel *byteLabel = nullptr;
        QLabel *bpsLabel = nullptr;
    };

    void buildUi();
    void rebuildForLanguage();
    void rememberBaseGeometry();
    void scaleToSize(const QSize &size);
    void setupRow(StreamRow &row, int index, QWidget *parent);
    void createBottomBar(QWidget *parent);
    void createActions();

    void onRowTypeChanged(int rowIndex, int comboIndex);
    void onRowOptClicked(int rowIndex);
    void onRowCmdClicked(int rowIndex);
    void onRowLogClicked(int rowIndex);
    void refreshRowButtons(StreamRow &row);
    void refreshMainButtons();
    void updateWindowTitle();
    void showContextMenu(const QPoint &pos);

    bool validateRow(const StreamRow &row);
    int startRow(StreamRow &row, bool asOutput);
    void removeChannels();

    void saveRowsToConfig();
    void loadRowsFromConfig();

    static ChannelType inputTypeForIndex(int index);
    static ChannelType outputTypeForIndex(int index);
    static void setIndicator(QLabel *ind, const QColor &color);

    StreamRow m_rows[4];

    QWidget *m_central = nullptr;
    QSize m_baseSize;
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnOptions = nullptr;
    QPushButton *m_btnExit = nullptr;

    // 记录控制条（原版顶部空白区域）
    QCheckBox *m_recordCheck = nullptr;
    QLineEdit *m_recordDirEdit = nullptr;
    QPushButton *m_recordBrowseBtn = nullptr;
    QLabel *m_recordStatusLabel = nullptr;

    QAction *m_actOptions = nullptr;
    QAction *m_actRecord = nullptr;
    QAction *m_actLanguage = nullptr;

    QTimer *m_statTimer = nullptr;
    bool m_running = false;
};

#endif // MAINWINDOW_H
