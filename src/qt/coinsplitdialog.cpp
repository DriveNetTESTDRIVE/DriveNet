#include <qt/coinsplitdialog.h>
#include <qt/forms/ui_coinsplitdialog.h>

#include <iostream>

#include <QMessageBox>

#include <base58.h>
#include <consensus/validation.h>
#include <net.h>
#include <primitives/transaction.h>

#include <wallet/coincontrol.h>
#include <wallet/wallet.h>
#include <validation.h>

// TODO this entire thing is a mess, clean up

CoinSplitDialog::CoinSplitDialog(const CAmount& amountIn, QString txidIn, QString formattedAmountIn, QString addressIn, int indexIn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CoinSplitDialog)
{
    ui->setupUi(this);

    amount = amountIn;
    txid = uint256S(txidIn.toStdString());
    index = indexIn;

    ui->labelTXID->setText(txidIn);
    ui->labelAmount->setText(formattedAmountIn);
    ui->labelAddress->setText(addressIn);
    ui->labelIndex->setText(QString::number(indexIn));

    QMessageBox messageBox;

    {
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
        messageBox.setText("Wallet must be unlocked to split coins.");
        messageBox.exec();
        return;
    }

    // Get new address
    if (vpwallets.empty())
        return;

    LOCK2(cs_main, vpwallets[0]->cs_wallet);
    vpwallets[0]->TopUpKeyPool();

    CPubKey newKey;
    if (vpwallets[0]->GetKeyFromPool(newKey)) {
        CKeyID keyID = newKey.GetID();

        CTxDestination address(keyID);
        strNewAddress = EncodeDestination(address);

        ui->labelNewAddress->setText(QString::fromStdString(strNewAddress));
    }
#endif
    }
}

CoinSplitDialog::~CoinSplitDialog()
{
    delete ui;
}

void CoinSplitDialog::on_buttonBox_accepted()
{
    QMessageBox messageBox;
    messageBox.setWindowTitle("Coin split error!");

    CTxDestination dest = DecodeDestination(strNewAddress);

    if (!IsValidDestination(dest)) {
        messageBox.setText("Invalid destination for split coins!");
        messageBox.exec();
        return;
    }

    // Try to split the coins
#ifdef ENABLE_WALLET
    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    CWalletTx wtx;
    // Force the wallet transaction to be version 3
    //CMutableTransaction mtx = *wtx.tx;
    //mtx.nVersion = 3;
    //wtx.SetTx(MakeTransactionRef(std::move(mtx)));

    CReserveKey reservekey(vpwallets[0]);
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    std::string strError = "";
    CCoinControl cc;
    cc.Select(COutPoint(txid, index));
    std::vector<CRecipient> vecSend;
    CRecipient recipient = {GetScriptForDestination(dest), amount, true};
    vecSend.push_back(recipient);
    if (!vpwallets[0]->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, cc, true, 3)) {
        QString message = "Failed to create coin split transaction!\n";
        message += "Error: ";
        message += QString::fromStdString(strError);
        message += "\n";
        messageBox.setText(message);
        messageBox.exec();
        return;
    }



    CValidationState state;
    if (!vpwallets[0]->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());

        QString message = "Failed to commit coin split transaction!\n";
        message += QString::fromStdString(strError);
        message += "\n";
        messageBox.setText(message);
        messageBox.exec();
        return;
    }

#endif

    messageBox.setWindowTitle("Coin split successfully!");
    QString message = "Your coin has been split and replay protected.\n";
    message += "txid: ";
    message += QString::fromStdString(wtx.GetHash().ToString());
    message += "\n";
    messageBox.setText(message);
    messageBox.exec();

    this->close();
}

void CoinSplitDialog::on_buttonBox_rejected()
{
    this->close();
}
