#include <qt/sidechainescrowtablemodel.h>

#include <qt/guiconstants.h>

#include <base58.h>
#include <pubkey.h>
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

Q_DECLARE_METATYPE(SidechainEscrowTableObject)

SidechainEscrowTableModel::SidechainEscrowTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    // This timer will be fired repeatedly to update the model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateModel()));
    pollTimer->start(MODEL_UPDATE_DELAY);
}

int SidechainEscrowTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int SidechainEscrowTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 7;
}

QVariant SidechainEscrowTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int col = index.column();
    int row = index.row();

    if (!model.at(row).canConvert<SidechainEscrowTableObject>())
        return QVariant();

    SidechainEscrowTableObject object = model.at(row).value<SidechainEscrowTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Escrow Number
        if (col == 0) {
            return object.nSidechain;
        }
        // Active
        if (col == 1) {
            return object.fActive;
        }
        // Escrow Name
        if (col == 2) {
            return object.name;
        }
        // Address
        if (col == 3) {
            return object.address;
        }
        // CTIP - TxID
        if (col == 4) {
            return object.CTIPTxID;
        }
        // CTIP - Index
        if (col == 5) {
            return object.CTIPIndex;
        }
        // Private key
        if (col == 6) {
            return object.privKey;
        }
    }
    }
    return QVariant();
}

QVariant SidechainEscrowTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("#");
            case 1:
                return QString("Active");
            case 2:
                return QString("Name");
            case 3:
                return QString("Address");
            case 4:
                return QString("CTIP TxID");
            case 5:
                return QString("CTIP Index");
            case 6:
                return QString("Private Key");
            }
        }
    }
    return QVariant();
}

void SidechainEscrowTableModel::updateModel()
{
#ifdef ENABLE_WALLET
    // Check for active wallet
    if (vpwallets.empty())
        return;

    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(vpwallets[0]->cs_wallet, lockWallet);
    if(!lockWallet)
        return;
#endif

    // TODO this is functional but not great

    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();

    int nSidechains = vSidechain.size();
    beginInsertRows(QModelIndex(), 0, nSidechains - 1);

    for (const Sidechain& s : vSidechain) {
        SidechainEscrowTableObject object;
        object.nSidechain = s.nSidechain;
        object.fActive = true; // TODO
        object.name = QString::fromStdString(s.GetSidechainName());

        // Sidechain deposit address
        CKeyID sidechainKey;
        sidechainKey.SetHex(s.sidechainKey);
        CSidechainAddress address;
        address.Set(sidechainKey);

        object.address = QString::fromStdString(address.ToString());
        object.privKey = QString::fromStdString(s.sidechainPriv);

        // Get the sidechain CTIP info
        {
            std::vector<COutput> vSidechainCoins;
            std::vector<unsigned char> vch(ParseHex(s.sidechainHex));
            CScript scriptPubKey = CScript(vch.begin(), vch.end());
#ifdef ENABLE_WALLET
            vpwallets[0]->AvailableSidechainCoins(scriptPubKey, s.nSidechain, vSidechainCoins);
#endif
            if (vSidechainCoins.size()) {
                object.CTIPIndex = QString::number(vSidechainCoins.front().i);
                object.CTIPTxID = QString::fromStdString(vSidechainCoins.front().tx->GetHash().ToString());
            } else {
                object.CTIPIndex = "NA";
                object.CTIPTxID = "NA";
            }
        }
        model.append(QVariant::fromValue(object));
    }

    endInsertRows();
}

void SidechainEscrowTableModel::AddDemoData()
{
    // Stop updating the model with real data
    pollTimer->stop();

    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();

    int nSidechains = vSidechain.size();
    beginInsertRows(QModelIndex(), 0, nSidechains - 1);

    for (const Sidechain& s : vSidechain) {
        SidechainEscrowTableObject object;
        object.nSidechain = s.nSidechain;
        object.fActive = true; // TODO
        object.name = QString::fromStdString(s.GetSidechainName());

        // Sidechain deposit address
        CKeyID sidechainKey;
        sidechainKey.SetHex(s.sidechainKey);
        CSidechainAddress address;
        address.Set(sidechainKey);

        object.address = QString::fromStdString(address.ToString());
        object.privKey = QString::fromStdString(s.sidechainPriv);

        // Add demo CTIP data
        object.CTIPIndex = QString::number(s.nSidechain % 2 == 0 ? 0 : 1);
        object.CTIPTxID = QString::fromStdString(GetRandHash().ToString());

        model.append(QVariant::fromValue(object));
    }

    endInsertRows();
}

void SidechainEscrowTableModel::ClearDemoData()
{
    // Clear demo data
    beginResetModel();
    model.clear();
    endResetModel();

    // Start updating the model with real data again
    pollTimer->start();
}
