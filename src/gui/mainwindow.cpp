#include "mainwindow.h"
#include "settingsdialog.h"
#include "serialdialog.h"
#include "networkdialog.h"

#include "../core/appconfig.h"
#include "../core/language.h"
#include "../core/logger.h"
#include "../core/rtcmrecorder.h"

#include <QCloseEvent>
#include <QResizeEvent>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStringList>
#include <QSize>
#include <QFrame>
#include <QWidget>
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QPalette>
#include <QPolygon>
#include <QCheckBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QColor>
#include <QApplication>
#include <QStyle>

namespace {

QIcon makePlayIcon()
{
    QPixmap pm(13, 13);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 128, 0));
    QPolygon poly;
    poly << QPoint(2, 1) << QPoint(11, 6) << QPoint(2, 11);
    p.drawPolygon(poly);
    p.end();
    return QIcon(pm);
}

QIcon makeStopIcon()
{
    QPixmap pm(13, 13);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.fillRect(2, 2, 9, 9, QColor(0, 0, 0));
    p.end();
    return QIcon(pm);
}

QStringList commandLines(const QString &text)
{
    QStringList result;
    const QStringList raw = text.split('\n');
    for (const QString &line : raw) {
        QString cmd = line;
        cmd.replace("\r", "");
        cmd = cmd.trimmed();
        if (!cmd.isEmpty()) result.append(cmd);
    }
    return result;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("STRSVR"));
    setMinimumSize(458, 232);   // 最小尺寸 = 原版客户区，可自由拖拽放大
    resize(458, 232);

    createActions();
    buildUi();
    loadRowsFromConfig();   // 恢复上次关闭时的通道配置（NTRIP 账号密码、串口参数等）

    // 每秒刷新 Bytes / Bps
    m_statTimer = new QTimer(this);
    connect(m_statTimer, &QTimer::timeout, this, &MainWindow::onRefreshStats);
    m_statTimer->start(1000);

    StreamManager *streamManager = &StreamManager::instance();
    connect(streamManager, &StreamManager::channelStarted,
            this, &MainWindow::onChannelStarted);
    connect(streamManager, &StreamManager::channelStopped,
            this, &MainWindow::onChannelStopped);
    connect(streamManager, &StreamManager::channelError,
            this, &MainWindow::onChannelError);

    RtcmRecorder *recorder = &RtcmRecorder::instance();
    connect(recorder, &RtcmRecorder::fileChanged,
            this, &MainWindow::updateWindowTitle);

    updateWindowTitle();
    LOG_INFO("MainWindow initialized");
}

MainWindow::~MainWindow()
{
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        StreamManager::instance().stopAll();
        removeChannels();
        m_running = false;
    }
    RtcmRecorder::instance().close();
    saveRowsToConfig();
    AppConfig::instance().save();   // 立即把 [streamN] 节落盘，不依赖 main 退出顺序
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 无菜单/工具栏/状态栏，中央部件与客户区同尺寸
    scaleToSize(event->size());
}

// ===== UI =====

void MainWindow::createActions()
{
    m_actOptions = new QAction(tr("&Options..."), this);
    connect(m_actOptions, &QAction::triggered, this, &MainWindow::onOptions);

    m_actRecord = new QAction(tr("Record RTCM data"), this);
    m_actRecord->setCheckable(true);
    m_actRecord->setShortcut(QKeySequence("Ctrl+R"));
    connect(m_actRecord, &QAction::triggered, this, &MainWindow::onToggleRecord);

    m_actLanguage = new QAction(QString(), this);
    m_actLanguage->setShortcut(QKeySequence("Ctrl+L"));
    connect(m_actLanguage, &QAction::triggered, this, &MainWindow::onToggleLanguage);

    addAction(m_actOptions);
    addAction(m_actRecord);
    addAction(m_actLanguage);
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    central->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(central, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    QPalette pal = central->palette();
    pal.setColor(QPalette::Window, Qt::white);
    central->setAutoFillBackground(true);
    central->setPalette(pal);

    QFont uiFont = central->font();
    uiFont.setPixelSize(13);
    central->setFont(uiFont);

    setCentralWidget(central);
    m_central = central;

    // ---- 记录控制条（原版顶部空白区） ----
    QWidget *recordBar = new QWidget(central);
    recordBar->setGeometry(1, 1, 456, 26);

    m_recordCheck = new QCheckBox(tr("Record RTCM data"), recordBar);
    m_recordCheck->setGeometry(2, 3, 150, 20);
    connect(m_recordCheck, &QCheckBox::toggled, this, &MainWindow::onRecordToggled);

    QLabel *dirLabel = new QLabel(tr("Directory:"), recordBar);
    dirLabel->setGeometry(155, 3, 42, 20);

    m_recordDirEdit = new QLineEdit(recordBar);
    m_recordDirEdit->setGeometry(199, 2, 132, 21);
    m_recordDirEdit->setText(RtcmRecorder::instance().directory());
    connect(m_recordDirEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_recordDirEdit) return;
        RtcmRecorder &rec = RtcmRecorder::instance();
        const QString dir = m_recordDirEdit->text().trimmed();
        if (dir.isEmpty() || dir == rec.directory()) return;
        rec.setDirectory(dir);
        rec.saveToConfig();
        updateWindowTitle();
    });

    m_recordBrowseBtn = new QPushButton("...", recordBar);
    m_recordBrowseBtn->setGeometry(333, 1, 24, 23);
    m_recordBrowseBtn->setFont(QFont(recordBar->font().family(), 8));
    connect(m_recordBrowseBtn, &QPushButton::clicked, this, &MainWindow::onRecordDirBrowse);

    m_recordStatusLabel = new QLabel("--", recordBar);
    m_recordStatusLabel->setGeometry(360, 3, 95, 20);
    QFont statusFont = m_recordStatusLabel->font();
    statusFont.setPixelSize(11);
    m_recordStatusLabel->setFont(statusFont);

    // ---- 通道区（原 Panel1） ----
    QFrame *panel1 = new QFrame(central);
    panel1->setGeometry(1, 29, 456, 132);
    panel1->setFrameShape(QFrame::Panel);
    panel1->setFrameShadow(QFrame::Sunken);
    panel1->setLineWidth(1);

    // 表头（原 Panel11）
    QWidget *header = new QWidget(panel1);
    header->setGeometry(2, 2, 452, 20);

    auto addHead = [header](const QString &text, int x, int w) {
        QLabel *l = new QLabel(text, header);
        l->setGeometry(x, 2, w, 16);
    };
    addHead(tr("Stream"), 35, 80);
    addHead(tr("Type"), 115, 55);
    addHead(tr("Opt"), 172, 26);
    addHead(tr("Cmd"), 198, 26);
    addHead(tr("Conv"), 224, 32);
    addHead(tr("Log"), 256, 30);
    addHead(tr("Bytes"), 350, 70);
    addHead(tr("Bps"), 426, 24);

    for (int i = 0; i < 4; ++i) {
        setupRow(m_rows[i], i, panel1);
    }

    createBottomBar(central);
    refreshMainButtons();

    // 记录基准几何（458x232），之后窗口缩放时按比例调整所有控件
    rememberBaseGeometry();
    scaleToSize(size());
}

void MainWindow::rememberBaseGeometry()
{
    if (!m_central) return;

    m_baseSize = QSize(458, 232);
    const QList<QWidget *> widgets = m_central->findChildren<QWidget *>();
    for (int i = 0; i < widgets.size(); ++i) {
        widgets[i]->setProperty("baseGeometry", widgets[i]->geometry());
    }
}

void MainWindow::scaleToSize(const QSize &size)
{
    if (!m_central || m_baseSize.isEmpty()) return;
    if (size.width() <= 0 || size.height() <= 0) return;

    const double sx = static_cast<double>(size.width()) / m_baseSize.width();
    const double sy = static_cast<double>(size.height()) / m_baseSize.height();

    const QList<QWidget *> widgets = m_central->findChildren<QWidget *>();
    for (int i = 0; i < widgets.size(); ++i) {
        QWidget *w = widgets[i];
        const QVariant v = w->property("baseGeometry");
        if (!v.isValid()) continue;

        const QRect g = v.toRect();
        w->setGeometry(qRound(g.x() * sx), qRound(g.y() * sy),
                       qRound(g.width() * sx), qRound(g.height() * sy));
    }
}

void MainWindow::setupRow(StreamRow &row, int index, QWidget *parent)
{
    row.index = index;

    row.panel = new QWidget(parent);
    row.panel->setGeometry(2, 22 + index * 28, 452, 28);

    QFont smallFont = row.panel->font();
    smallFont.setPixelSize(11);

    row.ind = new QLabel(row.panel);
    row.ind->setGeometry(6, 8, 12, 12);
    setIndicator(row.ind, QColor(200, 200, 200));

    row.label = new QLabel(row.panel);
    row.label->setGeometry(index == 0 ? 25 : 24, 7, 70, 16);
    row.label->setText(index == 0 ? tr("(0) Input") : tr("(%1) Output").arg(index));

    row.typeCombo = new QComboBox(row.panel);
    row.typeCombo->setGeometry(82, 3, 88, 21);
    row.typeCombo->setMaxVisibleItems(16);
    if (index == 0) {
        row.typeCombo->addItems(QStringList()
            << tr("Serial") << tr("TCP Client") << tr("TCP Server")
            << tr("NTRIP Client") << tr("UDP Server") << tr("File"));
    } else {
        row.typeCombo->addItems(QStringList()
            << QString() << tr("Serial") << tr("TCP Client") << tr("TCP Server")
            << tr("NTRIP Server") << tr("NTRIP Caster") << tr("UDP Client") << tr("File"));
    }
    row.typeCombo->setCurrentIndex(row.typeIndex);
    connect(row.typeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this, index](int ci) { onRowTypeChanged(index, ci); });

    row.btnOpt = new QPushButton("...", row.panel);
    row.btnOpt->setGeometry(172, 2, 25, 23);
    row.btnOpt->setFont(smallFont);
    connect(row.btnOpt, &QPushButton::clicked, this, [this, index]() { onRowOptClicked(index); });

    row.btnCmd = new QPushButton("...", row.panel);
    row.btnCmd->setGeometry(198, 2, 25, 23);
    row.btnCmd->setFont(smallFont);
    connect(row.btnCmd, &QPushButton::clicked, this, [this, index]() { onRowCmdClicked(index); });

    row.btnConv = nullptr;
    if (index != 0) {
        row.btnConv = new QPushButton("...", row.panel);
        row.btnConv->setGeometry(224, 2, 25, 23);
        row.btnConv->setFont(smallFont);
        row.btnConv->setEnabled(false);
        row.btnConv->setToolTip(tr("Conversion is not supported in this build"));
    }

    row.btnLog = new QPushButton("...", row.panel);
    row.btnLog->setGeometry(250, 2, 25, 23);
    row.btnLog->setFont(smallFont);
    connect(row.btnLog, &QPushButton::clicked, this, [this, index]() { onRowLogClicked(index); });

    row.indLog = new QLabel(row.panel);
    row.indLog->setGeometry(280, 8, 8, 12);
    setIndicator(row.indLog, QColor(200, 200, 200));

    row.byteLabel = new QLabel("0", row.panel);
    row.byteLabel->setGeometry(292, 7, 85, 16);
    row.byteLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    row.bpsLabel = new QLabel("0", row.panel);
    row.bpsLabel->setGeometry(378, 7, 65, 16);
    row.bpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    row.type = (index == 0) ? inputTypeForIndex(row.typeIndex)
                            : outputTypeForIndex(row.typeIndex);
    refreshRowButtons(row);
}

void MainWindow::createBottomBar(QWidget *parent)
{
    QWidget *bottom = new QWidget(parent);
    bottom->setGeometry(1, 201, 456, 30);

    // Options 与 Stop 共用同一位置，按运行状态切换显示（与原版一致）
    m_btnOptions = new QPushButton(bottom);
    m_btnOptions->setGeometry(153, 0, 151, 29);
    m_btnOptions->setText(tr("&Options..."));
    connect(m_btnOptions, &QPushButton::clicked, this, &MainWindow::onOptions);

    m_btnStart = new QPushButton(bottom);
    m_btnStart->setGeometry(1, 0, 151, 29);
    m_btnStart->setText(tr("&Start"));
    m_btnStart->setIcon(makePlayIcon());
    m_btnStart->setIconSize(QSize(13, 13));
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStart);

    m_btnStop = new QPushButton(bottom);
    m_btnStop->setGeometry(154, 0, 151, 29);
    m_btnStop->setText(tr("S&top"));
    m_btnStop->setIcon(makeStopIcon());
    m_btnStop->setIconSize(QSize(13, 13));
    m_btnStop->hide();
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::onStop);

    m_btnExit = new QPushButton(bottom);
    m_btnExit->setGeometry(304, 0, 151, 29);
    m_btnExit->setText(tr("E&xit"));
    connect(m_btnExit, &QPushButton::clicked, this, &QWidget::close);
}

void MainWindow::rebuildForLanguage()
{
    QWidget *old = takeCentralWidget();
    if (old) {
        delete old;
    }

    m_central = nullptr;
    m_btnStart = m_btnStop = m_btnOptions = m_btnExit = nullptr;
    m_recordCheck = nullptr;
    m_recordDirEdit = nullptr;
    m_recordBrowseBtn = nullptr;
    m_recordStatusLabel = nullptr;
    for (int i = 0; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        row.panel = nullptr;
        row.label = nullptr;
        row.typeCombo = nullptr;
        row.btnOpt = nullptr;
        row.btnCmd = nullptr;
        row.btnConv = nullptr;
        row.btnLog = nullptr;
        row.ind = nullptr;
        row.indLog = nullptr;
        row.byteLabel = nullptr;
        row.bpsLabel = nullptr;
    }

    buildUi();

    if (m_actOptions) m_actOptions->setText(tr("&Options..."));
    if (m_actRecord) m_actRecord->setText(tr("Record RTCM data"));

    for (int i = 0; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        if (row.channelId >= 0) {
            setIndicator(row.ind, QColor(0, 160, 0));
        }
        if (row.byteLabel) row.byteLabel->setText(QString::number(row.lastBytes));
        if (row.bpsLabel) row.bpsLabel->setText(QString::number(row.lastBps));
    }

    updateWindowTitle();
}

// ===== 行内按钮 =====

void MainWindow::onRowTypeChanged(int rowIndex, int comboIndex)
{
    StreamRow &row = m_rows[rowIndex];
    row.typeIndex = comboIndex;
    row.type = (row.index == 0) ? inputTypeForIndex(comboIndex)
                                : outputTypeForIndex(comboIndex);
    refreshRowButtons(row);
}

void MainWindow::onRowOptClicked(int rowIndex)
{
    StreamRow &row = m_rows[rowIndex];
    const bool isOutput = (row.index != 0);
    const int ci = row.typeCombo->currentIndex();
    const ChannelType type = isOutput ? outputTypeForIndex(ci) : inputTypeForIndex(ci);

    if (isOutput && (ci == 4 || ci == 5)) {
        QMessageBox::information(this, tr("STRSVR"),
            tr("NTRIP Server/Caster output is not supported in this build."));
        return;
    }

    switch (type) {
    case ChannelType::Serial: {
        SerialDialog dlg(this);
        dlg.setConfig(row.serial);
        if (dlg.exec() == QDialog::Accepted) {
            row.serial = dlg.config();
        }
        break;
    }
    case ChannelType::TcpClient: {
        NetworkDialog dlg(this);
        dlg.setCurrentPage(1);
        dlg.setTcpClientConfig(row.tcpClient);
        if (dlg.exec() == QDialog::Accepted) {
            row.tcpClient = dlg.tcpClientConfig();
        }
        break;
    }
    case ChannelType::TcpServer: {
        NetworkDialog dlg(this);
        dlg.setCurrentPage(0);
        dlg.setTcpServerPort(row.tcpServerPort);
        if (dlg.exec() == QDialog::Accepted) {
            row.tcpServerPort = dlg.tcpServerPort();
        }
        break;
    }
    case ChannelType::UdpServer: {
        NetworkDialog dlg(this);
        dlg.setCurrentPage(2);
        dlg.setUdpServerPort(row.udpServerPort);
        if (dlg.exec() == QDialog::Accepted) {
            row.udpServerPort = dlg.udpServerPort();
        }
        break;
    }
    case ChannelType::UdpClient: {
        NetworkDialog dlg(this);
        dlg.setCurrentPage(3);
        dlg.setUdpClientConfig(row.udpClient);
        if (dlg.exec() == QDialog::Accepted) {
            row.udpClient = dlg.udpClientConfig();
        }
        break;
    }
    case ChannelType::Ntrip: {
        NetworkDialog dlg(this);
        dlg.setCurrentPage(4);
        dlg.setNtripConfig(row.ntrip);
        if (dlg.exec() == QDialog::Accepted) {
            row.ntrip = dlg.ntripConfig();
        }
        break;
    }
    case ChannelType::File: {
        QString file;
        if (isOutput) {
            file = QFileDialog::getSaveFileName(this, tr("Select Output File"), row.filePath);
        } else {
            file = QFileDialog::getOpenFileName(this, tr("Select Input File"), row.filePath);
        }
        if (!file.isEmpty()) {
            row.filePath = file;
        }
        break;
    }
    default:
        break;
    }
    refreshRowButtons(row);
}

void MainWindow::onRowCmdClicked(int rowIndex)
{
    StreamRow &row = m_rows[rowIndex];

    bool ok = false;
    QString text = QInputDialog::getMultiLineText(
        this, tr("Startup Command"),
        tr("Commands sent when the stream starts (one per line):"),
        row.command, &ok);
    if (!ok) return;

    row.command = text;
    row.serial.startupCommands = commandLines(text);
}

void MainWindow::onRowLogClicked(int rowIndex)
{
    StreamRow &row = m_rows[rowIndex];

    QString file = QFileDialog::getSaveFileName(
        this, tr("Select Log File"), QDir::currentPath() + "/strsvr.log");
    if (file.isEmpty()) return;

    Logger::instance().setLogFile(file);
    setIndicator(row.indLog, QColor(0, 160, 0));
}

void MainWindow::refreshRowButtons(StreamRow &row)
{
    const bool configured = (row.index == 0) ? true : (row.typeIndex != 0);
    const bool idle = !m_running;

    if (row.typeCombo) row.typeCombo->setEnabled(idle);
    if (row.btnOpt) row.btnOpt->setEnabled(configured && idle);
    if (row.btnCmd) row.btnCmd->setEnabled(configured && idle);
    if (row.btnLog) row.btnLog->setEnabled(configured);
    if (row.btnConv) row.btnConv->setEnabled(false);
}

void MainWindow::refreshMainButtons()
{
    if (m_btnStart) m_btnStart->setVisible(!m_running);
    if (m_btnStop) m_btnStop->setVisible(m_running);
    if (m_btnOptions) m_btnOptions->setVisible(!m_running);

    for (int i = 0; i < 4; ++i) {
        refreshRowButtons(m_rows[i]);
    }
}

void MainWindow::updateWindowTitle()
{
    RtcmRecorder &rec = RtcmRecorder::instance();
    if (m_actRecord) m_actRecord->setChecked(rec.isEnabled());

    if (m_recordCheck) {
        QSignalBlocker blocker(m_recordCheck);
        m_recordCheck->setChecked(rec.isEnabled());
    }
    if (m_recordDirEdit) {
        const QString dir = rec.directory();
        if (m_recordDirEdit->text() != dir) {
            QSignalBlocker blocker(m_recordDirEdit);
            m_recordDirEdit->setText(dir);
        }
        m_recordDirEdit->setToolTip(dir);
    }

    if (rec.isEnabled()) {
        QString path = rec.currentFilePath();
        QString name = path.isEmpty()
                ? QDateTime::currentDateTimeUtc().date().toString("yyyyMMdd") + ".rtcm3"
                : QFileInfo(path).fileName();
        setWindowTitle(tr("STRSVR - REC %1").arg(name));
        if (m_recordStatusLabel) {
            m_recordStatusLabel->setText(name);
            m_recordStatusLabel->setToolTip(path.isEmpty() ? rec.directory() : path);
        }
    } else {
        setWindowTitle(tr("STRSVR"));
        if (m_recordStatusLabel) {
            m_recordStatusLabel->setText("--");
            m_recordStatusLabel->setToolTip(QString());
        }
    }

    if (m_actLanguage) {
        if (LanguageManager::instance().isChinese()) {
            m_actLanguage->setText(QString::fromUtf8("English"));
        } else {
            m_actLanguage->setText(QString::fromUtf8("中文"));
        }
    }
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    if (!m_central) return;

    QMenu menu(this);
    menu.addAction(m_actOptions);
    menu.addAction(m_actRecord);
    menu.addSeparator();
    menu.addAction(m_actLanguage);
    menu.addSeparator();
    QAction *quit = menu.addAction(tr("E&xit"));
    connect(quit, &QAction::triggered, this, &QWidget::close);
    menu.exec(m_central->mapToGlobal(pos));
}

// ===== 启停 =====

bool MainWindow::validateRow(const StreamRow &row)
{
    QString hint;
    switch (row.type) {
    case ChannelType::Serial:
        if (row.serial.portName.isEmpty()) hint = tr("serial port is not set");
        break;
    case ChannelType::TcpClient:
        if (row.tcpClient.host.isEmpty()) hint = tr("TCP client host is not set");
        break;
    case ChannelType::Ntrip:
        if (row.ntrip.host.isEmpty()) hint = tr("NTRIP caster host is not set");
        break;
    case ChannelType::File:
        if (row.filePath.isEmpty()) hint = tr("file is not set");
        break;
    default:
        break;
    }

    if (!hint.isEmpty()) {
        QMessageBox::warning(this, tr("STRSVR"),
            tr("%1: %2. Please click \"...\" to set the stream options.")
                .arg(row.label ? row.label->text() : QString(), hint));
        return false;
    }
    return true;
}

int MainWindow::startRow(StreamRow &row, bool asOutput)
{
    StreamManager &sm = StreamManager::instance();
    int id = sm.addChannel(row.type, row.label ? row.label->text() : QString());

    switch (row.type) {
    case ChannelType::Serial:
        sm.configureSerial(id, row.serial);
        break;
    case ChannelType::TcpServer:
        sm.configureTcpServer(id, row.tcpServerPort);
        break;
    case ChannelType::TcpClient:
        sm.configureTcpClient(id, row.tcpClient);
        break;
    case ChannelType::UdpServer:
        sm.configureUdpServer(id, row.udpServerPort);
        break;
    case ChannelType::UdpClient:
        sm.configureUdpClient(id, row.udpClient);
        break;
    case ChannelType::Ntrip:
        sm.configureNtrip(id, row.ntrip);
        break;
    case ChannelType::File:
        sm.configureFile(id, row.filePath, asOutput);
        break;
    default:
        break;
    }

    sm.enableChannel(id);

    row.channelId = id;
    row.lastBytes = 0;
    row.lastBps = 0;
    row.bpsTimer.restart();
    if (row.byteLabel) row.byteLabel->setText("0");
    if (row.bpsLabel) row.bpsLabel->setText("0");
    return id;
}

void MainWindow::removeChannels()
{
    StreamManager &sm = StreamManager::instance();
    for (int i = 0; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        if (row.channelId >= 0) {
            sm.removeChannel(row.channelId);
            row.channelId = -1;
        }
    }
}

void MainWindow::onStart()
{
    if (m_running) return;

    removeChannels();

    StreamRow &input = m_rows[0];
    input.typeIndex = input.typeCombo->currentIndex();
    input.type = inputTypeForIndex(input.typeIndex);

    if (!validateRow(input)) return;

    const int inputId = startRow(input, false);
    if (inputId < 0) return;

    int outputCount = 0;
    for (int i = 1; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        row.typeIndex = row.typeCombo->currentIndex();
        row.type = outputTypeForIndex(row.typeIndex);

        if (row.type == ChannelType::None) {
            if (row.typeIndex == 4 || row.typeIndex == 5) {
                LOG_WARN("Output %d: NTRIP Server/Caster is not supported", i);
            }
            continue;
        }

        if (!validateRow(row)) {
            removeChannels();
            return;
        }

        const int outputId = startRow(row, true);
        if (outputId >= 0) {
            StreamManager::instance().addRoute(inputId, outputId);
            ++outputCount;
        }
    }

    if (outputCount == 0) {
        QMessageBox::warning(this, tr("STRSVR"),
            tr("Please configure at least one output stream."));
        removeChannels();
        return;
    }

    m_running = true;
    refreshMainButtons();

    for (int i = 0; i < 4; ++i) {
        setIndicator(m_rows[i].ind,
                     m_rows[i].channelId >= 0 ? QColor(0, 160, 0) : QColor(200, 200, 200));
    }

    StreamManager::instance().startAll();

    LOG_INFO("Server started");
    updateWindowTitle();
}

void MainWindow::onStop()
{
    if (!m_running) return;

    StreamManager::instance().stopAll();
    removeChannels();
    m_running = false;
    refreshMainButtons();

    for (int i = 0; i < 4; ++i) {
        setIndicator(m_rows[i].ind, QColor(200, 200, 200));
    }

    LOG_INFO("Server stopped");
    updateWindowTitle();
}

void MainWindow::onOptions()
{
    const LanguageManager::Language before = LanguageManager::instance().current();

    SettingsDialog dlg(this);
    dlg.exec();

    if (LanguageManager::instance().current() != before) {
        rebuildForLanguage();
    }
    updateWindowTitle();
}

void MainWindow::onToggleLanguage()
{
    LanguageManager &lm = LanguageManager::instance();
    lm.setLang(lm.isChinese() ? LanguageManager::English : LanguageManager::Chinese);
    rebuildForLanguage();
}

void MainWindow::onToggleRecord()
{
    RtcmRecorder &rec = RtcmRecorder::instance();
    rec.setEnabled(!rec.isEnabled());
    rec.saveToConfig();
    updateWindowTitle();
}

void MainWindow::onRecordToggled(bool enabled)
{
    RtcmRecorder &rec = RtcmRecorder::instance();
    if (rec.isEnabled() != enabled) {
        rec.setEnabled(enabled);
        rec.saveToConfig();
    }
    updateWindowTitle();
}

void MainWindow::onRecordDirBrowse()
{
    RtcmRecorder &rec = RtcmRecorder::instance();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Record Directory"), rec.directory());
    if (dir.isEmpty()) return;

    rec.setDirectory(dir);
    rec.saveToConfig();
    updateWindowTitle();
}

// ===== 状态刷新 =====

void MainWindow::onRefreshStats()
{
    StreamManager &sm = StreamManager::instance();

    for (int i = 0; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        if (row.channelId < 0) continue;

        ChannelInfo *info = sm.channelInfo(row.channelId);
        if (!info) continue;

        const quint64 bytes = (row.index == 0) ? info->bytesReceived : info->bytesSent;

        const qint64 elapsed = row.bpsTimer.elapsed();
        if (elapsed >= 500) {
            row.lastBps = (bytes - row.lastBytes) * 1000 / static_cast<quint64>(elapsed);
            row.lastBytes = bytes;
            row.bpsTimer.restart();
        }

        if (row.byteLabel) row.byteLabel->setText(QString::number(bytes));
        if (row.bpsLabel) row.bpsLabel->setText(QString::number(row.lastBps));
    }
}

void MainWindow::onChannelStarted(int channelId)
{
    for (int i = 0; i < 4; ++i) {
        if (m_rows[i].channelId == channelId) {
            setIndicator(m_rows[i].ind, QColor(0, 160, 0));
            return;
        }
    }
}

void MainWindow::onChannelStopped(int channelId)
{
    for (int i = 0; i < 4; ++i) {
        if (m_rows[i].channelId == channelId) {
            setIndicator(m_rows[i].ind, QColor(200, 200, 200));
            return;
        }
    }
}

void MainWindow::onChannelError(int channelId, const QString &error)
{
    for (int i = 0; i < 4; ++i) {
        if (m_rows[i].channelId == channelId) {
            setIndicator(m_rows[i].ind, QColor(220, 0, 0));
            if (m_rows[i].ind) m_rows[i].ind->setToolTip(error);
            return;
        }
    }
}

// ===== 工具 =====

// 配置持久化：每个 StreamRow 用 [streamN] section 保存，strsvr.ini 同一文件
void MainWindow::saveRowsToConfig()
{
    AppConfig &cfg = AppConfig::instance();
    for (int i = 0; i < 4; ++i) {
        const StreamRow &row = m_rows[i];
        const QString group = QString("stream%1").arg(i);

        cfg.setInt(group, "typeindex", row.typeIndex);
        cfg.setString(group, "command", row.command);
        cfg.setString(group, "filepath", row.filePath);

        // Serial
        cfg.setString(group, "serial_port", row.serial.portName);
        cfg.setInt(group, "serial_baud", row.serial.baudRate);
        cfg.setInt(group, "serial_databits", static_cast<int>(row.serial.dataBits));
        cfg.setInt(group, "serial_parity", static_cast<int>(row.serial.parity));
        cfg.setInt(group, "serial_stopbits", static_cast<int>(row.serial.stopBits));
        cfg.setInt(group, "serial_flowctrl", static_cast<int>(row.serial.flowControl));
        cfg.setString(group, "serial_cmds", row.serial.startupCommands.join('|'));

        // TCP Client
        cfg.setString(group, "tcpclient_host", row.tcpClient.host);
        cfg.setInt(group, "tcpclient_port", row.tcpClient.port);

        // TCP Server
        cfg.setInt(group, "tcpserver_port", row.tcpServerPort);

        // UDP Server
        cfg.setInt(group, "udpserver_port", row.udpServerPort);

        // UDP Client
        cfg.setString(group, "udpclient_host", row.udpClient.host);
        cfg.setInt(group, "udpclient_port", row.udpClient.port);

        // NTRIP
        cfg.setString(group, "ntrip_host", row.ntrip.host);
        cfg.setInt(group, "ntrip_port", row.ntrip.port);
        cfg.setString(group, "ntrip_mountpoint", row.ntrip.mountpoint);
        cfg.setString(group, "ntrip_user", row.ntrip.user);
        cfg.setString(group, "ntrip_password", row.ntrip.password);
        cfg.setBool(group, "ntrip_sendgga", row.ntrip.sendGga);
        cfg.setBool(group, "ntrip_ggafromserial", row.ntrip.ggaFromSerial);
        cfg.setDouble(group, "ntrip_ggalat", row.ntrip.ggaLat);
        cfg.setDouble(group, "ntrip_ggalon", row.ntrip.ggaLon);
        cfg.setDouble(group, "ntrip_ggaheight", row.ntrip.ggaHeight);

        LOG_INFO("Saved %s: typeIndex=%d ntrip=%s:%d/%s user=%s sendGga=%d autoGga=%d",
                 group.toStdString().c_str(), row.typeIndex,
                 row.ntrip.host.toStdString().c_str(), row.ntrip.port,
                 row.ntrip.mountpoint.toStdString().c_str(),
                 row.ntrip.user.toStdString().c_str(),
                 row.ntrip.sendGga ? 1 : 0, row.ntrip.ggaFromSerial ? 1 : 0);
    }
    LOG_INFO("Stream rows saved to config");
}

void MainWindow::loadRowsFromConfig()
{
    AppConfig &cfg = AppConfig::instance();
    for (int i = 0; i < 4; ++i) {
        StreamRow &row = m_rows[i];
        const QString group = QString("stream%1").arg(i);

        // 没有这个 section 说明是第一次运行，跳过。用一个极小哨兵区分“不存在”和有效值
        const int ti = cfg.getInt(group, "typeindex", -999999);
        if (ti == -999999) continue;
        row.typeIndex = ti;
        row.type = (row.index == 0) ? inputTypeForIndex(row.typeIndex)
                                    : outputTypeForIndex(row.typeIndex);
        row.command = cfg.getString(group, "command", "");
        row.filePath = cfg.getString(group, "filepath", "");

        row.serial.portName = cfg.getString(group, "serial_port", "");
        row.serial.baudRate = cfg.getInt(group, "serial_baud", 115200);
        row.serial.dataBits = static_cast<QSerialPort::DataBits>(
            cfg.getInt(group, "serial_databits", static_cast<int>(QSerialPort::Data8)));
        row.serial.parity = static_cast<QSerialPort::Parity>(
            cfg.getInt(group, "serial_parity", static_cast<int>(QSerialPort::NoParity)));
        row.serial.stopBits = static_cast<QSerialPort::StopBits>(
            cfg.getInt(group, "serial_stopbits", static_cast<int>(QSerialPort::OneStop)));
        row.serial.flowControl = static_cast<QSerialPort::FlowControl>(
            cfg.getInt(group, "serial_flowctrl", static_cast<int>(QSerialPort::NoFlowControl)));
        const QString cmdJoined = cfg.getString(group, "serial_cmds", "");
        if (!cmdJoined.isEmpty()) row.serial.startupCommands = cmdJoined.split('|');

        row.tcpClient.host = cfg.getString(group, "tcpclient_host", "");
        row.tcpClient.port = static_cast<quint16>(cfg.getInt(group, "tcpclient_port", 0));

        row.tcpServerPort = static_cast<quint16>(cfg.getInt(group, "tcpserver_port", 2101));

        row.udpServerPort = static_cast<quint16>(cfg.getInt(group, "udpserver_port", 2101));

        row.udpClient.host = cfg.getString(group, "udpclient_host", "");
        row.udpClient.port = static_cast<quint16>(cfg.getInt(group, "udpclient_port", 0));

        row.ntrip.host = cfg.getString(group, "ntrip_host", "");
        row.ntrip.port = static_cast<quint16>(cfg.getInt(group, "ntrip_port", 0));
        row.ntrip.mountpoint = cfg.getString(group, "ntrip_mountpoint", "");
        row.ntrip.user = cfg.getString(group, "ntrip_user", "");
        row.ntrip.password = cfg.getString(group, "ntrip_password", "");
        row.ntrip.sendGga = cfg.getBool(group, "ntrip_sendgga", true);
        row.ntrip.ggaFromSerial = cfg.getBool(group, "ntrip_ggafromserial", true);
        row.ntrip.ggaLat = cfg.getDouble(group, "ntrip_ggalat", 0.0);
        row.ntrip.ggaLon = cfg.getDouble(group, "ntrip_ggalon", 0.0);
        row.ntrip.ggaHeight = cfg.getDouble(group, "ntrip_ggaheight", 0.0);

        // 同步 UI 下拉框（setupRow 已创建，按 typeIndex 选中）
        if (row.typeCombo) {
            row.typeCombo->blockSignals(true);
            row.typeCombo->setCurrentIndex(row.typeIndex);
            row.typeCombo->blockSignals(false);
        }
        refreshRowButtons(row);

        LOG_INFO("Loaded %s: typeIndex=%d ntrip=%s:%d/%s user=%s sendGga=%d autoGga=%d",
                 group.toStdString().c_str(), row.typeIndex,
                 row.ntrip.host.toStdString().c_str(), row.ntrip.port,
                 row.ntrip.mountpoint.toStdString().c_str(),
                 row.ntrip.user.toStdString().c_str(),
                 row.ntrip.sendGga ? 1 : 0, row.ntrip.ggaFromSerial ? 1 : 0);
    }
    LOG_INFO("Stream rows loaded from config");
}

ChannelType MainWindow::inputTypeForIndex(int index)
{
    switch (index) {
    case 0: return ChannelType::Serial;
    case 1: return ChannelType::TcpClient;
    case 2: return ChannelType::TcpServer;
    case 3: return ChannelType::Ntrip;
    case 4: return ChannelType::UdpServer;
    case 5: return ChannelType::File;
    default: return ChannelType::None;
    }
}

ChannelType MainWindow::outputTypeForIndex(int index)
{
    switch (index) {
    case 0: return ChannelType::None;
    case 1: return ChannelType::Serial;
    case 2: return ChannelType::TcpClient;
    case 3: return ChannelType::TcpServer;
    case 6: return ChannelType::UdpClient;
    case 7: return ChannelType::File;
    default: return ChannelType::None;   // 4=NTRIP Server, 5=NTRIP Caster（暂不支持）
    }
}

void MainWindow::setIndicator(QLabel *ind, const QColor &color)
{
    if (!ind) return;
    ind->setStyleSheet(QString("background-color:%1; border:1px solid #808080;")
                       .arg(color.name()));
}
