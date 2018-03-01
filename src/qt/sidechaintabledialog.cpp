#include <qt/sidechaintabledialog.h>

#include <qt/sidechainescrowtablemodel.h>
#include <qt/sidechainwithdrawaltablemodel.h>
#include <qt/forms/ui_sidechaintabledialog.h>

#include <QHeaderView>
#include <QScrollBar>

#include <chain.h>
#include <chainparams.h>
#include <validation.h>

SidechainTableDialog::SidechainTableDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainTableDialog)
{
    ui->setupUi(this);

    // Initialize models
    escrowModel = new SidechainEscrowTableModel(this);
    withdrawalModel = new SidechainWithdrawalTableModel(this);

    // Add models to table views
    ui->tableViewD1->setModel(escrowModel);
    ui->tableViewD2->setModel(withdrawalModel);

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableViewD1->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewD2->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableViewD1->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewD2->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Don't stretch last cell of horizontal header
    ui->tableViewD1->horizontalHeader()->setStretchLastSection(false);
    ui->tableViewD2->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewD1->verticalHeader()->setVisible(false);
    ui->tableViewD2->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewD1->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tableViewD2->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewD1->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewD2->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewD1->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels
    ui->tableViewD2->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Disable word wrap
    ui->tableViewD1->setWordWrap(false);
    ui->tableViewD2->setWordWrap(false);
}

SidechainTableDialog::~SidechainTableDialog()
{
    delete ui;
}

void SidechainTableDialog::on_pushButtonClose_clicked()
{
    this->close();
}

void SidechainTableDialog::on_pushButtonTest_clicked()
{
    escrowModel->AddDemoData();
    withdrawalModel->AddDemoData();
}

void SidechainTableDialog::on_pushButtonClear_clicked()
{
    escrowModel->ClearDemoData();
    withdrawalModel->ClearDemoData();
}
