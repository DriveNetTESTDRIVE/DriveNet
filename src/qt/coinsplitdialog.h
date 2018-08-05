#ifndef COINSPLITDIALOG_H
#define COINSPLITDIALOG_H

#include <amount.h>
#include <uint256.h>
#include <script/standard.h>

#include <QDialog>
#include <QString>
#include <QWidget>

namespace Ui {
class CoinSplitDialog;
}

class CoinSplitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinSplitDialog(const CAmount& amountIn, QString txidIn, QString formattedAmountIn, QString addressIn, int indexIn, QWidget *parent = 0);
    ~CoinSplitDialog();

public Q_SLOTS:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::CoinSplitDialog *ui;

    CAmount amount;
    uint256 txid;
    int index;
    std::string strNewAddress;

};

#endif // COINSPLITDIALOG_H
