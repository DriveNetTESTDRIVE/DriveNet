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
#include <random.h>
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
    std::string strHash = ui->lineEditHash->text().toStdString();
    std::string strHashID1 = ui->lineEditIDHash1->text().toStdString();
    std::string strHashID2 = ui->lineEditIDHash2->text().toStdString();
    int nVersion = ui->spinBoxVersion->value();

    if (strTitle.empty()) {
        QMessageBox::critical(this, tr("DriveNet - error"),
                              tr("Sidechain must have a title!"),
                              QMessageBox::Ok);
        return;
    }

    // TODO maybe we should allow sidechains with no description? Anyways this
    // isn't a consensus rule right now
    if (strDescription.empty()) {
        QMessageBox::critical(this, tr("DriveNet - error"),
                              tr("Sidechain must have a description!"),
                              QMessageBox::Ok);
        return;
    }

    if (nVersion > SIDECHAIN_VERSION_MAX) {
        QMessageBox::critical(this, tr("DriveNet - error"),
                              tr("This sidechain has an invalid version number (too high)!"),
                              QMessageBox::Ok);
        return;
    }

    uint256 uHash = uint256S(strHash);
    if (uHash.IsNull()) {
        QMessageBox::critical(this, tr("DriveNet - error"),
                              tr("Invalid sidechain build commit hash!"),
                              QMessageBox::Ok);
        return;
    }

    CKey key;
    key.Set(uHash.begin(), uHash.end(), false);

    CBitcoinSecret vchSecret(key);

    if (!key.IsValid()) {
        // Nobody should see this, but we don't want to fail silently
        QMessageBox::critical(this, tr("DriveNet - error"),
                              tr("Private key outside allowed range!"),
                              QMessageBox::Ok);
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
    proposal.sidechainKeyID = HexStr(vchAddress);
    proposal.sidechainHex = HexStr(sidechainScript);
    proposal.hashID1 = uint256S(strHashID1);
    proposal.hashID2 = uint256S(strHashID2);
    proposal.nVersion = nVersion;

    // Cache proposal so that it can be added to the next block we mine
    scdb.CacheSidechainProposals(std::vector<SidechainProposal>{proposal});

    QString message = QString("Sidechain proposal created!\n\n");
    message += QString("Version:\n%1\n\n").arg(nVersion);
    message += QString("Title:\n%1\n\n").arg(QString::fromStdString(strTitle));
    message += QString("Description:\n%1\n\n").arg(QString::fromStdString(strDescription));
    message += QString("Private key:\n%1\n\n").arg(QString::fromStdString(proposal.sidechainPriv));
    message += QString("KeyID:\n%1\n\n").arg(QString::fromStdString(proposal.sidechainKeyID));
    message += QString("Deposit script:\n%1\n\n").arg(QString::fromStdString(proposal.sidechainHex));
    if (!strHashID1.empty())
        message += QString("Hash ID 1:\n%1\n\n").arg(QString::fromStdString(strHashID1));
    if (!strHashID2.empty())
        message += QString("Hash ID 2:\n%1\n\n").arg(QString::fromStdString(strHashID2));

    QMessageBox::information(this, tr("DriveNet - sidechain proposal created!"),
                          message,
                          QMessageBox::Ok);

    // TODO clear out line edits and text box
    ui->lineEditTitle->clear();
    ui->plainTextEditDescription->clear();
    ui->lineEditHash->clear();
    ui->lineEditIDHash1->clear();
    ui->lineEditIDHash2->clear();
    ui->spinBoxVersion->setValue(0);
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
    // TODO move text into static const char *
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("Use this page to ACK (acknowledgement) or "
                                      "NACK (negative-acknowledgement) sidechains.\n\n"
                                      "Set ACK to activate a proposed sidechain, "
                                      "and NACK to reject a proposed sidechain.\n\n"
                                      "Once set, the chosen signal will be included "
                                      "in blocks mined by this node."),
                                   QMessageBox::Ok);
}

void SidechainMinerDialog::on_toolButtonKeyHash_clicked()
{
    // TODO move text into static const char *
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("Sidechain key hash:\n\n"
                                      "Enter any SHA256 hash. This hash will be "
                                      "used to generate the sidechain private key."
                                      ),
                                   QMessageBox::Ok);
}

void SidechainMinerDialog::on_toolButtonIDHash1_clicked()
{
    // TODO display message based on current selected version
    // TODO move text into static const char *
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("Releae tarball:\n\n"
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

void SidechainMinerDialog::on_toolButtonIDHash2_clicked()
{
    // TODO display message based on current selected version
    // TODO move text into static const char *
    QMessageBox::information(this, tr("DriveNet - information"),
                                   tr("Build commit hash:\n\n"
                                      "This is the commit which the gitian "
                                      "release was built with."
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

void SidechainMinerDialog::on_pushButtonRandomKeyHash_clicked()
{
    uint256 hash = GetRandHash();
    ui->lineEditHash->setText(QString::fromStdString(hash.ToString()));
}
