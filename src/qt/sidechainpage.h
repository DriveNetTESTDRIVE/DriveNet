// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINPAGE_H
#define SIDECHAINPAGE_H

#include <QWidget>

#include <amount.h>
#include <uint256.h>

#include <string>

class CBlock;
class WalletModel;

class SidechainEscrowTableModel;
class SidechainWithdrawalTableModel;

namespace Ui {
class SidechainPage;
}

class SidechainPage : public QWidget
{
    Q_OBJECT

public:
    explicit SidechainPage(QWidget *parent = 0);
    ~SidechainPage();

    void setWalletModel(WalletModel *model);

public Q_SLOTS:
    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                    const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                    const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

private:
    Ui::SidechainPage *ui;

    WalletModel *walletModel;

    SidechainEscrowTableModel *escrowModel;
    SidechainWithdrawalTableModel *withdrawalModel;

    void SetupTables();
};

#endif // SIDECHAINPAGE_H
