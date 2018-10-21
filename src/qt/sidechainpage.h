// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINPAGE_H
#define SIDECHAINPAGE_H

#include <QString>
#include <QWidget>

#include <amount.h>
#include <sidechain.h>
#include <uint256.h>

#include <string>

class CBlock;
class WalletModel;

class SidechainEscrowTableModel;
class SidechainWithdrawalTableModel;

namespace Ui {
class SidechainPage;
}

// Sidechain icons
static const std::array<QString, VALID_SIDECHAINS_COUNT> SidechainIcons =
{{
    {":/icons/sidechain_one"},
    {":/icons/sidechain_payments"},
}};

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

    void on_pushButtonDeposit_clicked();

    void on_pushButtonPaste_clicked();

    void on_pushButtonClear_clicked();

    void on_comboBoxSidechains_currentIndexChanged(const int index);

    void on_listWidgetSidechains_doubleClicked(const QModelIndex& index);

private:
    Ui::SidechainPage *ui;

    WalletModel *walletModel;

    SidechainEscrowTableModel *escrowModel;
    SidechainWithdrawalTableModel *withdrawalModel;

    void SetupTables();

    bool validateDepositAmount();

};

#endif // SIDECHAINPAGE_H
