#include <qt/sidechainwithdrawaltablemodel.h>

#include <qt/guiconstants.h>

#include <random.h>
#include <sidechain.h>
#include <sidechaindb.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <math.h>

#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

#include <base58.h>
#include <script/standard.h>
#include <qt/guiutil.h>

#include <qt/drivenetaddressvalidator.h>
#include <qt/drivenetunits.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <primitives/transaction.h>
#include <init.h>
#include <policy/policy.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <util.h>

Q_DECLARE_METATYPE(SidechainWithdrawalTableObject)

SidechainWithdrawalTableModel::SidechainWithdrawalTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    // This timer will be fired repeatedly to update the model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateModel()));
    pollTimer->start(MODEL_UPDATE_DELAY);
}

int SidechainWithdrawalTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int SidechainWithdrawalTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 6;
}

QVariant SidechainWithdrawalTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(row).canConvert<SidechainWithdrawalTableObject>())
        return QVariant();

    SidechainWithdrawalTableObject object = model.at(row).value<SidechainWithdrawalTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Sidechain name
        if (col == 0) {
            return object.sidechain;
        }
        // Age
        if (col == 1) {
            return object.nAge;
        }
        // Max age
        if (col == 2) {
            return object.nMaxAge;
        }
        // Acks
        if (col == 3) {
            return object.nAcks;
        }
        // Approved
        if (col == 4) {
            return object.fApproved;
        }
        // WT^ hash
        if (col == 5) {
            return object.hashWTPrime;
        }
    }
    }
    return QVariant();
}

QVariant SidechainWithdrawalTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Sidechain");
            case 1:
                return QString("Age");
            case 2:
                return QString("Max Age");
            case 3:
                return QString("Acks");
            case 4:
                return QString("Approved");
            case 5:
                return QString("WT^ hash");
            }
        }
    }
    return QVariant();
}

void SidechainWithdrawalTableModel::updateModel()
{
    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    if (!scdb.HasState())
        return;

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();

    int nSidechains = vSidechain.size();
    beginInsertColumns(QModelIndex(), model.size(), model.size() + nSidechains);
    for (const Sidechain& s : vSidechain) {
        std::vector<SidechainWTPrimeState> vState = scdb.GetState(s.nSidechain);
        for (const SidechainWTPrimeState& wt : vState) {
            SidechainWithdrawalTableObject object;
            object.sidechain = QString::fromStdString(s.GetSidechainName());
            object.hashWTPrime = QString::fromStdString(wt.hashWTPrime.ToString());
            object.nAcks = wt.nWorkScore;
            object.nAge = abs(wt.nBlocksLeft - SIDECHAIN_VERIFICATION_PERIOD) + 1; // Note +1 because zero based
            object.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
            object.fApproved = scdb.CheckWorkScore(wt.nSidechain, wt.hashWTPrime);

            model.append(QVariant::fromValue(object));
        }
    }
    endInsertColumns();
}

void SidechainWithdrawalTableModel::AddDemoData()
{
    // Stop updating the model with real data
    pollTimer->stop();

    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, 5);

    // WT^ 1
    SidechainWithdrawalTableObject object1;
    object1.sidechain = QString::fromStdString("Grin");
    object1.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object1.nAcks = 42;
    object1.nAge = 50;
    object1.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object1.fApproved = false;

    // WT^ 2
    SidechainWithdrawalTableObject object2;
    object2.sidechain = QString::fromStdString("Hivemind");
    object2.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object2.nAcks = 13141;
    object2.nAge = 21358;
    object2.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object2.fApproved = true;

    // WT^ 3
    SidechainWithdrawalTableObject object3;
    object3.sidechain = QString::fromStdString("Hivemind");
    object3.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object3.nAcks = 1637;
    object3.nAge = 2000;
    object3.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object3.fApproved = false;

    // WT^ 4
    SidechainWithdrawalTableObject object4;
    object4.sidechain = QString::fromStdString("Cash");
    object4.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object4.nAcks = 705;
    object4.nAge = 26215;
    object4.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object4.fApproved = false;

    // WT^ 5
    SidechainWithdrawalTableObject object5;
    object5.sidechain = QString::fromStdString("Hivemind");
    object5.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object5.nAcks = 10;
    object5.nAge = 10;
    object5.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object5.fApproved = false;

    // WT^ 6
    SidechainWithdrawalTableObject object6;
    object6.sidechain = QString::fromStdString("sofa");
    object6.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object6.nAcks = 1256;
    object6.nAge = 1378;
    object6.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object6.fApproved = false;

    // WT^ 7
    SidechainWithdrawalTableObject object7;
    object7.sidechain = QString::fromStdString("Cash");
    object7.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object7.nAcks = SIDECHAIN_MIN_WORKSCORE + 10;
    object7.nAge = SIDECHAIN_MIN_WORKSCORE + 11;
    object7.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object7.fApproved = true;

    // WT^ 8
    SidechainWithdrawalTableObject object8;
    object8.sidechain = QString::fromStdString("Hivemind");
    object8.hashWTPrime = QString::fromStdString(GetRandHash().ToString());
    object8.nAcks = 1;
    object8.nAge = 26142;
    object8.nMaxAge = SIDECHAIN_VERIFICATION_PERIOD;
    object8.fApproved = false;

    // Add demo objects to model
    model.append(QVariant::fromValue(object1));
    model.append(QVariant::fromValue(object2));
    model.append(QVariant::fromValue(object3));
    model.append(QVariant::fromValue(object4));
    model.append(QVariant::fromValue(object5));
    model.append(QVariant::fromValue(object6));
    model.append(QVariant::fromValue(object7));
    model.append(QVariant::fromValue(object8));

    endInsertRows();
}

void SidechainWithdrawalTableModel::ClearDemoData()
{
    // Clear demo data
    beginResetModel();
    model.clear();
    endResetModel();

    // Start updating the model with real data again
    pollTimer->start();
}
