#include "aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About STRSVR"));
    setFixedSize(420, 320);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QLabel *titleLabel = new QLabel(tr("STRSVR"), this);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel *verLabel = new QLabel(tr("GNSS Stream Server - Linux Port v1.0.0"), this);
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setStyleSheet("color: #34495e;");
    mainLayout->addWidget(verLabel);

    mainLayout->addSpacing(10);

    // Description
    QLabel *descLabel = new QLabel(this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setText(tr(
        "A data stream server for GNSS applications.\n"
        "Supports NMEA 0183 and CMR protocols,\n"
        "Serial ports, TCP/UDP sockets, NTRIP clients."
    ));
    descLabel->setStyleSheet("color: #7f8c8d;");
    mainLayout->addWidget(descLabel);

    mainLayout->addSpacing(15);

    // Copyright
    QLabel *copyrightLabel = new QLabel(this);
    copyrightLabel->setWordWrap(true);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setText(tr(
        "Original STRSVR Copyright (C) 2007-2017 T.Takasu\n\n"
        "This is an independent C++/Qt5 reimplementation\n"
        "not affiliated with the original author.\n\n"
        "This program is provided AS IS without warranty."
    ));
    copyrightLabel->setStyleSheet("color: #95a5a6; font-size: 11px;");
    mainLayout->addWidget(copyrightLabel);

    mainLayout->addStretch();

    // Technical info
    QLabel *techLabel = new QLabel(this);
    techLabel->setAlignment(Qt::AlignCenter);
    techLabel->setStyleSheet("color: #95a5a6; font-size: 10px;");
    techLabel->setText(tr("Built with C++17, Qt 5.15+"));
    mainLayout->addWidget(techLabel);

    // Close button
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}
