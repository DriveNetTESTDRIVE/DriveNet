// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINDEPOSITDIALOG_H
#define SIDECHAINDEPOSITDIALOG_H

#include <QDialog>

namespace Ui {
class SidechainDepositDialog;
}

class SidechainDepositDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainDepositDialog(QWidget *parent = 0);
    ~SidechainDepositDialog();

private Q_SLOTS:
    void on_pushButtonDeposit_clicked();

    void on_pushButtonPaste_clicked();

    void on_pushButtonClear_clicked();

private:
    Ui::SidechainDepositDialog *ui;

    bool validateDepositAmount();
};

#endif // SIDECHAINDEPOSITDIALOG_H
