#ifndef SIDECHAINACTIVATIONTABLEMODEL_H
#define SIDECHAINACTIVATIONTABLEMODEL_H

#include <uint256.h>

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct SidechainActivationTableObject
{
    bool fActivate;
    QString title;
    QString description;
    QString sidechainKeyID;
    QString sidechainHex;
    QString sidechainPriv;
    int nAge;
    int nFail;
    QString hash;
};

class SidechainActivationTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SidechainActivationTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    bool GetHashAtRow(int row, uint256& hash) const;

public Q_SLOTS:
    void updateModel();

private:
    QList<QVariant> model;
    QTimer *pollTimer;
};

#endif // SIDECHAINACTIVATIONTABLEMODEL_H
