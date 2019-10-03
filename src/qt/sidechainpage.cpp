// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainpage.h>
#include <qt/forms/ui_sidechainpage.h>

#include <qt/drivenetunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/sidechaindepositconfirmationdialog.h>
#include <qt/sidechainescrowtablemodel.h>
#include <qt/sidechainwithdrawaltablemodel.h>
#include <qt/sidechainminerdialog.h>
#include <qt/walletmodel.h>

#include <base58.h>
#include <coins.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <miner.h>
#include <net.h>
#include <primitives/block.h>
#include <sidechaindb.h>
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
#include <QTimer>

#include <sstream>

SidechainPage::SidechainPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SidechainPage)
{
    ui->setupUi(this);

    ui->listWidgetSidechains->setIconSize(QSize(32, 32));

    // Setup sidechain list widget & combo box
    SetupSidechainList();

    // Setup the tables
    SetupTables();

    // Initialize deposit confirmation dialog
    depositConfirmationDialog = new SidechainDepositConfirmationDialog(this);

    // Initialize miner popup window. We want users to be able to keep this
    // window open while using the rest of the software.
    minerDialog = new SidechainMinerDialog(this);

    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(CheckForSidechainUpdates()));
    pollTimer->start(MODEL_UPDATE_DELAY);
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

QString SidechainPage::GetSidechainIconPath(uint8_t nSidechain) const
{
    return ":/icons/sidechain_default";
}

void SidechainPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    // TODO use all of these values
    // int unit = walletModel->getOptionsModel()->getDisplayUnit();
    // const CAmount& pending = immatureBalance + unconfirmedBalance;
    // ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    // ui->pending->setText(BitcoinUnits::formatWithUnit(unit, pending, false, BitcoinUnits::separatorAlways));
}

void SidechainPage::SetupSidechainList()
{
    // Setup Sidechains list widget
    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();

    // If there are no active sidechains, display message
    if (vSidechain.empty())
        ui->stackedWidgetSecondary->setCurrentIndex(1);
    else
        ui->stackedWidgetSecondary->setCurrentIndex(0);

    // Remove any existing list widget items
    ui->listWidgetSidechains->clear();

    // Update the list widget with new sidechains
    for (const Sidechain& s : vSidechain) {
        QListWidgetItem *item = new QListWidgetItem(ui->listWidgetSidechains);

        // Set icon
        QIcon icon(GetSidechainIconPath(s.nSidechain));
        item->setIcon(icon);

        // Set text
        item->setText(QString::fromStdString(scdb.GetSidechainName(s.nSidechain)));
        QFont font = item->font();
        font.setPointSize(16);
        item->setFont(font);

        ui->listWidgetSidechains->addItem(item);
    }

    // Remove any existing sidechains from the selection box
    ui->comboBoxSidechains->clear();

    // Setup sidechain selection combo box
    for (const Sidechain& s : vSidechain) {
        ui->comboBoxSidechains->addItem(QString::fromStdString(scdb.GetSidechainName(s.nSidechain)));
    }

    ui->listWidgetSidechains->setCurrentRow(0);
}

void SidechainPage::SetupTables()
{
    if (escrowModel)
        delete escrowModel;
    if (withdrawalModel)
        delete withdrawalModel;

    // Initialize table models
    escrowModel = new SidechainEscrowTableModel(this);
    withdrawalModel = new SidechainWithdrawalTableModel(this);

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

void SidechainPage::on_pushButtonDeposit_clicked()
{
    QMessageBox messageBox;

    unsigned int nSidechain = ui->comboBoxSidechains->currentIndex();

    if (!IsSidechainNumberValid(nSidechain)) {
        // Should never be displayed
        messageBox.setWindowTitle("Invalid sidechain selected");
        messageBox.exec();
        return;
    }

    if (!validateDepositAmount()) {
        // Invalid deposit amount message box
        messageBox.setWindowTitle("Invalid deposit amount!");
        QString error = "Check the amount you have entered and try again.\n\n";
        error += "Your deposit must be > 0.00001 BTC to cover the sidechain ";
        error += "deposit fee. If the output amount is dust after paying the ";
        error += "fee, you will not receive anything on the sidechain.\n";
        messageBox.setText(error);
        messageBox.exec();
        return;
    }

    if (!validateFeeAmount()) {
        // Invalid fee amount message box
        messageBox.setWindowTitle("Invalid fee amount!");
        QString error = "Check the fee you have entered and try again.\n\n";
        error += "Your fee must be greater than 0 & not dust!\n";
        messageBox.setText(error);
        messageBox.exec();
        return;
    }

    // Get keyID
    CSidechainAddress address(ui->payTo->text().toStdString());
    CKeyID keyID;
    if (!address.GetKeyID(keyID)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid sidechain address!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Get fee and deposit amount
    const CAmount& nValue = ui->payAmount->value();
    const CAmount& nFee = ui->feeAmount->value();

    // Format strings for confirmation dialog
    QString strSidechain = QString::fromStdString(scdb.GetSidechainName(nSidechain));
    QString strValue = BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, nValue, false, BitcoinUnits::separatorAlways);
    QString strFee = BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, nFee, false, BitcoinUnits::separatorAlways);

    // Once we've made it to this point and validated what we can, show the
    // deposit confirmation dialog and check the result.
    // Note that GetConfirmed() will automatically reset the dialog
    depositConfirmationDialog->SetInfo(strSidechain, strValue, strFee);
    depositConfirmationDialog->exec();
    if (!depositConfirmationDialog->GetConfirmed())
        return;

#ifdef ENABLE_WALLET
    if (vpwallets.empty()) {
        messageBox.setWindowTitle("Wallet Error!");
        messageBox.setText("No active wallets to create the deposit.");
        messageBox.exec();
        return;
    }

    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to create sidechain deposit.");
        messageBox.exec();
        return;
    }

    // Attempt to create the deposit
    CTransactionRef tx;
    std::string strFail = "";
    if (!vpwallets.empty()) {

        CScript scriptPubKey;
        if (!scdb.GetSidechainScript(nSidechain, scriptPubKey)) {
            // Invalid sidechain message box
            messageBox.setWindowTitle("Invalid Sidechain!");
            messageBox.setText("The sidechain you're trying to deposit to does not appear to be active!");
            messageBox.exec();
            return;
        }
        if (!vpwallets[0]->CreateSidechainDeposit(tx, strFail, scriptPubKey, nSidechain, nValue, keyID)) {
            // Create transaction error message box
            messageBox.setWindowTitle("Creating deposit transaction failed!");
            QString createError = "Error creating transaction!\n\n";
            createError += QString::fromStdString(strFail);
            messageBox.setText(createError);
            messageBox.exec();
            return;
        }
    }

    // Successful deposit message box
    messageBox.setWindowTitle("Deposit transaction created!");
    QString result = "Deposited to " + strSidechain;
    result += "\n";
    result += "txid: " + QString::fromStdString(tx->GetHash().ToString());
    result += "\n";
    result += "Amount deposited: ";
    result += BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, nValue, false, BitcoinUnits::separatorAlways);
    messageBox.setText(result);
    messageBox.exec();
#endif
}

void SidechainPage::on_pushButtonPaste_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SidechainPage::on_pushButtonClear_clicked()
{
    ui->payTo->clear();
}

void SidechainPage::on_comboBoxSidechains_currentIndexChanged(const int i)
{
    if (!IsSidechainNumberValid(i))
        return;

    ui->listWidgetSidechains->setCurrentRow(i);

    // Update deposit button text
    QString strSidechain = QString::fromStdString(scdb.GetSidechainName(i));
    QString str = "Deposit to: " + strSidechain;
    ui->pushButtonDeposit->setText(str);
}

void SidechainPage::on_listWidgetSidechains_doubleClicked(const QModelIndex& i)
{
    ui->comboBoxSidechains->setCurrentIndex(i.row());
}

bool SidechainPage::validateDepositAmount()
{
    if (!ui->payAmount->validate()) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject deposits which cannot cover sidechain fee
    if (ui->payAmount->value() < SIDECHAIN_DEPOSIT_FEE) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject deposits which would net the user no payout on the sidechain
    if (GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value() - SIDECHAIN_DEPOSIT_FEE)) {
        ui->payAmount->setValid(false);
        return false;
    }

    return true;
}

bool SidechainPage::validateFeeAmount()
{
    if (!ui->feeAmount->validate()) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->feeAmount->value(0) <= 0) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->feeAmount->value())) {
        ui->feeAmount->setValid(false);
        return false;
    }

    return true;
}

void SidechainPage::on_pushButtonManageSidechains_clicked()
{
    minerDialog->show();
}

void SidechainPage::CheckForSidechainUpdates()
{
    std::vector<Sidechain> vSidechainNew = scdb.GetActiveSidechains();
    if (vSidechainNew != vSidechain) {
        vSidechain = vSidechainNew;

        SetupSidechainList();
        SetupTables();
    }
}
