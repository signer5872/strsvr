/**
 * NetworkDialog - TCP/UDP/NTRIP configuration
 */

#ifndef NETWORKDIALOG_H
#define NETWORKDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

#include "../io/network.h"

class NetworkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkDialog(QWidget *parent = nullptr);
    ~NetworkDialog() override = default;

    TcpClientWorker::Config tcpClientConfig() const;
    UdpClientWorker::Config udpClientConfig() const;
    NtripClientWorker::Config ntripConfig() const;
    quint16 tcpServerPort() const;
    quint16 udpServerPort() const;

    void setTcpClientConfig(const TcpClientWorker::Config &cfg);
    void setUdpClientConfig(const UdpClientWorker::Config &cfg);
    void setNtripConfig(const NtripClientWorker::Config &cfg);
    void setTcpServerPort(quint16 port);
    void setUdpServerPort(quint16 port);

    /** 打开时定位到指定页：0=TCP Server,1=TCP Client,2=UDP Server,3=UDP Client,4=NTRIP */
    void setCurrentPage(int index);

private:
    void buildUi();
    void updateGgaLatLonState();

    QTabWidget *m_tabWidget = nullptr;

    // TCP Server
    QSpinBox *m_tcpServerPortSpin = nullptr;

    // TCP Client
    QLineEdit *m_tcpClientHostEdit = nullptr;
    QSpinBox *m_tcpClientPortSpin = nullptr;
    QCheckBox *m_tcpClientReconnectCheck = nullptr;
    QSpinBox *m_tcpClientReconnectIntervalSpin = nullptr;

    // UDP Server
    QSpinBox *m_udpServerPortSpin = nullptr;

    // UDP Client
    QLineEdit *m_udpClientHostEdit = nullptr;
    QSpinBox *m_udpClientPortSpin = nullptr;

    // NTRIP Client (bonus)
    QLineEdit *m_ntripHostEdit = nullptr;
    QSpinBox *m_ntripPortSpin = nullptr;
    QLineEdit *m_ntripMountpointEdit = nullptr;
    QLineEdit *m_ntripUserEdit = nullptr;
    QLineEdit *m_ntripPasswordEdit = nullptr;
    // VRS GGA 上传
    QCheckBox *m_ntripGgaCheck = nullptr;
    QLineEdit *m_ntripLatEdit = nullptr;
    QLineEdit *m_ntripLonEdit = nullptr;
    QLineEdit *m_ntripHeightEdit = nullptr;
    QCheckBox *m_ntripGgaAutoCheck = nullptr;
};

#endif // NETWORKDIALOG_H
