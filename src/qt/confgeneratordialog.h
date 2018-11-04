#ifndef CONFGENERATORDIALOG_H
#define CONFGENERATORDIALOG_H

#include <QDialog>

namespace Ui {
class ConfGeneratorDialog;
}

class ConfGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfGeneratorDialog(QWidget *parent = 0);
    ~ConfGeneratorDialog();

private Q_SLOTS:
    void on_pushButtonClose_clicked();
    void on_pushButtonApply_clicked();
    void on_pushButtonRandom_clicked();

private:
    Ui::ConfGeneratorDialog *ui;

    bool WriteConfigFiles(const QString& strUser, const QString& strPass);
};

#endif // CONFGENERATORDIALOG_H
