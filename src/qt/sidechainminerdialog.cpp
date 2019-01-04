// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainminerdialog.h>
#include <qt/forms/ui_sidechainminerdialog.h>

#include <QMessageBox>
#include <QScrollBar>

#include <qt/confgeneratordialog.h>
#include <qt/drivenetunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/sidechainactivationtablemodel.h>
#include <qt/walletmodel.h>

#include <key.h>
#include <wallet/wallet.h>
#include <sidechain.h>
#include <sidechaindb.h>
#include <validation.h>

static const unsigned int INDEX_VOTE_SIDECHAIN = 0;
static const unsigned int INDEX_PROPOSE_SIDECHAIN = 1;
static const unsigned int INDEX_VOTE_WTPRIME = 2;
static const unsigned int INDEX_BMM_SETTINGS = 3;
static const unsigned int INDEX_CONFIG = 4;

SidechainMinerDialog::SidechainMinerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainMinerDialog)
{
    ui->setupUi(this);

    activationModel = new SidechainActivationTableModel(this);

    ui->tableViewActivation->setModel(activationModel);

    // Don't stretch last cell of horizontal header
    ui->tableViewActivation->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewActivation->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewActivation->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewActivation->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewActivation->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Select entire row
    ui->tableViewActivation->setSelectionBehavior(QAbstractItemView::SelectRows);
}

SidechainMinerDialog::~SidechainMinerDialog()
{
    delete ui;
}

void SidechainMinerDialog::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel())
    {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this,
                SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
    }
}

void SidechainMinerDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    const CAmount& pending = immatureBalance + unconfirmedBalance;
    //ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    //ui->pending->setText(BitcoinUnits::formatWithUnit(unit, pending, false, BitcoinUnits::separatorAlways));
}

void SidechainMinerDialog::on_pushButtonVoteSidechain_clicked()
{
    ui->stackedWidget->setCurrentIndex(INDEX_VOTE_SIDECHAIN);
}

void SidechainMinerDialog::on_pushButtonProposeSidechain_clicked()
{
    ui->stackedWidget->setCurrentIndex(INDEX_PROPOSE_SIDECHAIN);
}

void SidechainMinerDialog::on_pushButtonVoteWTPrime_clicked()
{
    ui->stackedWidget->setCurrentIndex(INDEX_VOTE_WTPRIME);
}

void SidechainMinerDialog::on_pushButtonCreateSidechainProposal_clicked()
{
    std::string strTitle = ui->lineEditTitle->text().toStdString();
    std::string strDescription = ui->plainTextEditDescription->toPlainText().toStdString();
    std::string strHash= ui->lineEditHash->text().toStdString();

    if (strTitle.empty()) {
        // TODO show error message
        //"Sidechain must have a title!"
        return;
    }

    // TODO maybe we should allow sidechains with no description? Anyways this
    // isn't a consensus rule right now
    if (strDescription.empty()) {
        // TODO show error message
        // "Sidechain must have a description!"
        return;
    }

    uint256 uHash = uint256S(strHash);
    if (uHash.IsNull()) {
        // TODO show error message
        // "Invalid sidechain release hash!"
        return;
    }

    CKey key;
    key.Set(uHash.begin(), uHash.end(), false);

    CBitcoinSecret vchSecret(key);

//    CKey key = vchSecret.GetKey();
    if (!key.IsValid()) {
        // TODO show error message
        // "Private key outside allowed range");
        return;
    }

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();

    // Generate script hex
    CScript sidechainScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(vchAddress) << OP_EQUALVERIFY << OP_CHECKSIG;

    SidechainProposal proposal;
    proposal.title = strTitle;
    proposal.description = strDescription;
    proposal.sidechainPriv = vchSecret.ToString();
    proposal.sidechainKey = HexStr(vchAddress);
    proposal.sidechainHex = HexStr(sidechainScript);

    // Cache proposal so that it can be added to the next block we mine
    scdb.CacheSidechainProposals(std::vector<SidechainProposal>{proposal});



    // TODO display result message
    // TODO clear out line edits and text box
}

void SidechainMinerDialog::on_pushButtonActivate_clicked()
{
    QModelIndexList selected = ui->tableViewActivation->selectionModel()->selectedIndexes();

    for (int i = 0; i < selected.size(); i++) {
        uint256 hash;
        if (activationModel->GetHashAtRow(selected[i].row(), hash))
            scdb.CacheSidechainHashToActivate(hash);
    }
}

void SidechainMinerDialog::on_pushButtonReject_clicked()
{
    QModelIndexList selected = ui->tableViewActivation->selectionModel()->selectedIndexes();

    for (int i = 0; i < selected.size(); i++) {
        uint256 hash;
        if (activationModel->GetHashAtRow(selected[i].row(), hash))
            scdb.RemoveSidechainHashToActivate(hash);
    }
}

void SidechainMinerDialog::on_pushButtonClose_clicked()
{
    this->close();
}

void SidechainMinerDialog::on_toolButtonACKSidechains_clicked()
{
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("Use this page to ACK (acknowledgement) or "
                                      "NACK (negative-acknowledgement) sidechains.\n\n"
                                      "Set ACK to activate a proposed sidechain, "
                                      "and NACK to reject a proposed sidechain.\n\n"
                                      "Once set, the chosen signal will be included "
                                      "in blocks mined by this node."),
                                   QMessageBox::Ok);
}

void SidechainMinerDialog::on_toolButtonReleaseHash_clicked()
{
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("The deterministic build release hash is the "
                                      "hash of the original gitian software build "
                                      "of this sidechain.\n\n"
                                      "Use the sha256sum utility to generate this "
                                      "hash, or copy the hash when it is printed "
                                      "to the console after gitian builds complete.\n\n"
                                      "Example:\n"
                                      "sha256sum DriveNet-12-0.21.00-x86_64-linux-gnu.tar.gz\n\n"
                                      "Result:\n"
                                      "fd9637e427f1e967cc658bfe1a836d537346ce3a6dd0746878129bb5bc646680  DriveNet-12-0.21.00-x86_64-linux-gnu.tar.gz\n\n"
                                      "Paste the resulting hash into the field."
                                      ),
                                   QMessageBox::Ok);
}

void SidechainMinerDialog::on_pushButtonGenerateConfig_clicked()
{
    // Show configuration generator dialog
    ConfGeneratorDialog *dialog = new ConfGeneratorDialog(this);
    dialog->exec();
}

void SidechainMinerDialog::on_pushButtonConfigFiles_clicked()
{
    ui->stackedWidget->setCurrentIndex(INDEX_CONFIG);
}

