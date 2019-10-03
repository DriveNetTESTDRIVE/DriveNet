// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDEPOSITCONFIRMATIONDIALOG_H
#define BITCOIN_SIDECHAINDEPOSITCONFIRMATIONDIALOG_H

#include <QDialog>

#include <amount.h>

namespace Ui {
class SidechainDepositConfirmationDialog;
}

class SidechainDepositConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainDepositConfirmationDialog(QWidget *parent = 0);
    ~SidechainDepositConfirmationDialog();

    bool GetConfirmed();
    void SetInfo(const QString& strSidechain, const QString& strAmount, const QString& strFee);

public Q_SLOTS:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::SidechainDepositConfirmationDialog *ui;
    void Reset();

    bool fConfirmed = false;
};

#endif // BITCOIN_SIDECHAINDEPOSITCONFIRMATIONDIALOG_H
