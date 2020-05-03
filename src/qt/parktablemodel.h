#ifndef PARKTABLEMODEL_H
#define PARKTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

class CWallet;
class WalletModel;
class ParkTableModelPriv;

class ParkTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ParkTableModel(CWallet* wallet, WalletModel *parent = 0);
    ~ParkTableModel();

    enum ColumnIndex {
        Status = 0,
        Date = 1,
        Amount = 2,
        EstimatedTimeLeft = 3,
        Premium = 4,
        UnparkAddress = 5,
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;

private:
    CWallet* wallet;
    WalletModel *walletModel;
    QStringList columns;
    ParkTableModelPriv *priv;
    unsigned int lastUpdateBestHeight;

signals:

public slots:
    void update();

    friend class ParkTableModelPriv;
};

#endif // PARKTABLEMODEL_H
