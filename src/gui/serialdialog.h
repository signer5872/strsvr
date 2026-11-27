/**
 * SerialDialog - Serial port configuration
 */

#ifndef SERIALDIALOG_H
#define SERIALDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QPushButton>

#include "../io/serialport.h"

class SerialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SerialDialog(QWidget *parent = nullptr);
    ~SerialDialog() override = default;

    SerialPortWorker::Config config() const;
    void setConfig(const SerialPortWorker::Config &config);

private slots:
    void refreshPorts();

private:
    void buildUi();

    QComboBox *m_portCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_flowControlCombo = nullptr;
    QLineEdit *m_startupCmdsEdit = nullptr;
};

#endif // SERIALDIALOG_H
