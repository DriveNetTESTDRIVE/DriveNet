// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
 * includes we might want here


#include <qt/sidechainminerdialog.h>
#include <qt/forms/ui_sidechainminerdialog.h>

#include <QMessageBox>
#include <QScrollBar>

#include <qt/drivenetunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/sidechainactivationtablemodel.h>
#include <qt/walletmodel.h>

#include <core_io.h>
#include <key.h>
#include <wallet/wallet.h>
#include <random.h>
#include <sidechain.h>
#include <sidechaindb.h>
#include <validation.h>
*/

#include <qt/sidechaindepositconfirmationdialog.h>
#include <qt/forms/ui_sidechaindepositconfirmationdialog.h>

#include <utilmoneystr.h>

SidechainDepositConfirmationDialog::SidechainDepositConfirmationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainDepositConfirmationDialog)
{
    ui->setupUi(this);
    Reset();
}

SidechainDepositConfirmationDialog::~SidechainDepositConfirmationDialog()
{
    delete ui;
}

bool SidechainDepositConfirmationDialog::GetConfirmed()
{
    // Return the confirmation status and reset dialog
    if (fConfirmed) {
        Reset();
        return true;
    } else {
        Reset();
        return false;
    }
}

void SidechainDepositConfirmationDialog::SetInfo(const QString& strSidechain, const QString& strAmount, const QString& strFee)
{
    ui->labelSidechain->setText(strSidechain);
    ui->labelAmount->setText(strAmount);
    ui->labelFee->setText(strFee);
}

void SidechainDepositConfirmationDialog::Reset()
{
    // Reset the dialog's fields and confirmation status
    fConfirmed = false;
}

void SidechainDepositConfirmationDialog::on_buttonBox_accepted()
{
    fConfirmed = true;
    this->close();
}

void SidechainDepositConfirmationDialog::on_buttonBox_rejected()
{
    this->close();
}
