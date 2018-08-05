// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainpage.h>
#include <qt/forms/ui_sidechainpage.h>

#include <qt/drivenetunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/sidechainescrowtablemodel.h>
#include <qt/sidechainwithdrawaltablemodel.h>
#include <qt/walletmodel.h>

#include <base58.h>
#include <coins.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <miner.h>
#include <net.h>
#include <primitives/block.h>
#include <sidechain.h>
#include <txdb.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>
#include <QStackedWidget>

#include <sstream>

SidechainPage::SidechainPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SidechainPage)
{
    ui->setupUi(this);

    ui->listWidgetSidechains->setIconSize(QSize(32, 32));

    // Setup Sidechains list widget
    for (const Sidechain& s : ValidSidechains) {
        QListWidgetItem *item = new QListWidgetItem(ui->listWidgetSidechains);

        // Set icon
        QIcon icon(":/icons/bitcoin");
        item->setIcon(icon);

        // Set text
        item->setText(QString::fromStdString(s.GetSidechainName()));
        QFont font = item->font();
        font.setPointSize(22);
        item->setFont(font);

        ui->listWidgetSidechains->addItem(item);
    }

    // Initialize models
    escrowModel = new SidechainEscrowTableModel(this);
    withdrawalModel = new SidechainWithdrawalTableModel(this);

    // Setup the tables
    SetupTables();

    ui->listWidgetSidechains->setCurrentRow(0);
}

SidechainPage::~SidechainPage()
{
    delete ui;
}

void SidechainPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel())
    {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this,
                SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
    }
}

void SidechainPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    const CAmount& pending = immatureBalance + unconfirmedBalance;
    //ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    //ui->pending->setText(BitcoinUnits::formatWithUnit(unit, pending, false, BitcoinUnits::separatorAlways));
}

void SidechainPage::SetupTables()
{
    // Add models to table views
    ui->tableViewEscrow->setModel(escrowModel);
    ui->tableViewWT->setModel(withdrawalModel);

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableViewEscrow->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewWT->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableViewEscrow->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewWT->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Don't stretch last cell of horizontal header
    ui->tableViewEscrow->horizontalHeader()->setStretchLastSection(false);
    ui->tableViewWT->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewEscrow->verticalHeader()->setVisible(false);
    ui->tableViewWT->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewEscrow->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tableViewWT->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewEscrow->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewWT->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewEscrow->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels
    ui->tableViewWT->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Disable word wrap
    ui->tableViewEscrow->setWordWrap(false);
    ui->tableViewWT->setWordWrap(false);
}
