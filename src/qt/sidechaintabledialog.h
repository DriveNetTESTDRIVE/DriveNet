#ifndef SIDECHAINTABLEDIALOG_H
#define SIDECHAINTABLEDIALOG_H

#include <QDialog>

class SidechainEscrowTableModel;
class SidechainWithdrawalTableModel;

namespace Ui {
class SidechainTableDialog;
}

class SidechainTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainTableDialog(QWidget *parent = 0);
    ~SidechainTableDialog();

private Q_SLOTS:
    void on_pushButtonClose_clicked();
    void on_pushButtonTest_clicked();
    void on_pushButtonClear_clicked();

private:
    Ui::SidechainTableDialog *ui;
    SidechainEscrowTableModel *escrowModel;
    SidechainWithdrawalTableModel *withdrawalModel;

};

#endif // SIDECHAINTABLEDIALOG_H
