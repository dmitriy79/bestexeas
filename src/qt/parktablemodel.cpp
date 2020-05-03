#include "parktablemodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"

#include <QTimer>
#include <QIcon>

using namespace std;

class ParkRecord
{
public:
    uint256 hash;
    unsigned int n;
    int64 time;
    int64 amount;
    int64 duration;
    std::string unparkAddress;
    int depth;
    bool premiumKnown;
    int64 premium;
    int64 remaining;
    CBlockIndex* blockIndex;
    unsigned int lastUpdateBestHeight;

    static bool extract(const CWallet* wallet, const COutPoint& out, ParkRecord& record)
    {
        std::map<uint256, CWalletTx>::const_iterator mi = wallet->mapWallet.find(out.hash);
        if (mi == wallet->mapWallet.end())
            return false;

        const CWalletTx& wtx = mi->second;

        if (wtx.vout.size() <= out.n)
            return false;

        if (wtx.IsSpent(out.n))
            return false;

        const CTxOut& txo = wtx.vout[out.n];

        int64 nDuration;
        CTxDestination unparkAddress;

        if (!ExtractPark(txo.scriptPubKey, nDuration, unparkAddress))
            return false;

        if (!wallet->IsMine(txo))
            return false;

        record.hash = out.hash;
        record.n = out.n;

        record.time = wtx.nTime;
        record.amount = txo.nValue;
        record.duration = nDuration;
        record.unparkAddress = CBitcoinAddress(unparkAddress, wtx.cUnit).ToString();

        CBlockIndex *pindex = NULL;
        record.depth = wtx.GetDepthInMainChain(pindex);

        if (record.depth >= record.duration)
            record.remaining = 0;
        else
            record.remaining = record.duration - record.depth;

        if (pindex)
        {
            record.premiumKnown = true;
            record.premium = pindex->GetPremium(record.amount, record.duration, wtx.cUnit);
        }
        else
        {
            record.premiumKnown = false;
            record.premium = 0;
        }

        record.blockIndex = pindex;
        record.lastUpdateBestHeight = nBestHeight;

        return true;
    }

    static QList<ParkRecord> decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx)
    {
        QList<ParkRecord> records;
        for (unsigned int i = 0; i < wtx.vout.size(); i++)
        {
            ParkRecord record;
            if (ParkRecord::extract(wallet, COutPoint(wtx.GetHash(), i), record))
                records.append(record);
        }
        return records;
    }

    bool updateNeeded()
    {
        return nBestHeight != lastUpdateBestHeight;
    }

    void update(const CWallet* wallet)
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = wallet->mapWallet.find(hash);
        if (mi == wallet->mapWallet.end())
            return;

        const CWalletTx& wtx = mi->second;

        if (wtx.vout.size() <= n)
            return;

        const CTxOut& txo = wtx.vout[n];

        CBlockIndex *pindex = NULL;
        depth = wtx.GetDepthInMainChain(pindex);

        if (depth >= duration)
            remaining = 0;
        else
            remaining = duration - depth;

        if (pindex)
        {
            premiumKnown = true;
            if (pindex != blockIndex)
                premium = pindex->GetPremium(amount, duration, wtx.cUnit);
        }
        else
        {
            premiumKnown = false;
            premium = 0;
        }

        blockIndex = pindex;
    }
};

// Comparison operator for sort/binary search of model tx list
struct TxLessThan
{
    bool operator()(const ParkRecord &a, const ParkRecord &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const ParkRecord &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const ParkRecord &b) const
    {
        return a < b.hash;
    }
};

class ParkTableModelPriv
{
public:
    ParkTableModelPriv(CWallet *wallet, ParkTableModel *parent):
            wallet(wallet),
            parent(parent)
    {
    }
    CWallet *wallet;
    ParkTableModel *parent;

    QList<ParkRecord> records;

    void init()
    {
        parent->beginResetModel();
        records.clear();
        {
            LOCK2(cs_main, wallet->cs_wallet);

            for (set<COutPoint>::const_iterator it = wallet->setParked.begin(); it != wallet->setParked.end(); ++it)
            {
                ParkRecord record;
                if (ParkRecord::extract(wallet, *it, record))
                    records.append(record);
            }
        }
        parent->endResetModel();
    }

    void update(QList<uint256> updated)
    {
        // Sort update list, and iterate through it in reverse, so that model updates
        //  can be emitted from end to beginning (so that earlier updates will not influence
        // the indices of latter ones).
        QList<uint256> updated_sorted = updated;
        qSort(updated_sorted);

        {
            LOCK2(cs_main, wallet->cs_wallet);
            for(int update_idx = updated_sorted.size()-1; update_idx >= 0; --update_idx)
            {
                const uint256 &hash = updated_sorted.at(update_idx);
                // Find transaction in wallet
                std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
                bool inWallet = mi != wallet->mapWallet.end();
                // Find bounds of this transaction in model
                QList<ParkRecord>::iterator lower = qLowerBound(
                    records.begin(), records.end(), hash, TxLessThan());
                QList<ParkRecord>::iterator upper = qUpperBound(
                    records.begin(), records.end(), hash, TxLessThan());
                int lowerIndex = (lower - records.begin());
                int upperIndex = (upper - records.begin());

                // Determine if transaction is in model already
                bool inModel = false;
                if(lower != upper)
                {
                    inModel = true;
                }

                if(inWallet && !inModel)
                {
                    // Added -- insert at the right position
                    QList<ParkRecord> toInsert =
                            ParkRecord::decomposeTransaction(wallet, mi->second);
                    if(!toInsert.isEmpty()) /* only if something to insert */
                    {
                        parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex+toInsert.size()-1);
                        int insert_idx = lowerIndex;
                        foreach(const ParkRecord &rec, toInsert)
                        {
                            records.insert(insert_idx, rec);
                            insert_idx += 1;
                        }
                        parent->endInsertRows();
                    }
                }
                else if(!inWallet && inModel)
                {
                    // Removed -- remove entire transaction from table
                    parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
                    records.erase(lower, upper);
                    parent->endRemoveRows();
                }
                else if(inWallet && inModel)
                {
                    // Updated. Remove spent (unparked) outputs
                    const CWalletTx& wtx = mi->second;
                    for (int i = upperIndex - 1; i >= lowerIndex; i--)
                    {
                        ParkRecord& record = records[i];

                        if (wtx.IsSpent(record.n))
                        {
                            parent->beginRemoveRows(QModelIndex(), i, i);
                            records.removeAt(i);
                            parent->endRemoveRows();
                        }
                    }
                }
            }
        }
    }

    int size()
    {
        return records.size();
    }

    ParkRecord *index(int idx)
    {
        if(idx >= 0 && idx < records.size())
        {
            ParkRecord *rec = &records[idx];
            if (rec->updateNeeded())
                rec->update(wallet);
            return rec;
        }
        else
        {
            return 0;
        }
    }
};

ParkTableModel::ParkTableModel(CWallet* wallet, WalletModel *parent):
        QAbstractTableModel(parent),
        wallet(wallet),
        walletModel(parent),
        priv(new ParkTableModelPriv(wallet, this)),
        lastUpdateBestHeight(0)
{
    columns << "" << tr("Date") << tr("Amount") << tr("Est. time left") << tr("Premium") << tr("Unpark address");

    priv->init();

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(MODEL_UPDATE_DELAY);
}

ParkTableModel::~ParkTableModel()
{
    delete priv;
}

void ParkTableModel::update()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    QList<uint256> updated;

    // Check if there are changes to wallet map
    {
        TRY_LOCK(wallet->cs_wallet, lockWallet);
        if (lockWallet && !wallet->vParkWalletUpdated.empty())
        {
            BOOST_FOREACH(uint256 hash, wallet->vParkWalletUpdated)
            {
                updated.append(hash);
            }
            wallet->vParkWalletUpdated.clear();
        }
    }

    if(!updated.empty())
    {
        priv->update(updated);
    }

    bool updateNeeded;
    {
        if (nBestHeight != lastUpdateBestHeight)
        {
            lastUpdateBestHeight = nBestHeight;
            updateNeeded = true;
        }
        else
            updateNeeded = false;
    }
    if (updateNeeded)
    {
        emit dataChanged(index(0, Status), index(priv->size()-1, Status));
        emit dataChanged(index(0, Premium), index(priv->size()-1, Premium));
    }
}

int ParkTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int ParkTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant ParkTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    ParkRecord *rec = static_cast<ParkRecord*>(index.internalPointer());

    switch(role)
    {
    case Qt::DecorationRole:
        switch(index.column())
        {
        case Status:
            switch(rec->depth)
            {
            case 0: return QIcon(":/icons/transaction_0");
            case 1: return QIcon(":/icons/transaction_1");
            case 2: return QIcon(":/icons/transaction_2");
            case 3: return QIcon(":/icons/transaction_3");
            case 4: return QIcon(":/icons/transaction_4");
            case 5: return QIcon(":/icons/transaction_5");
            default: return QIcon(":/icons/transaction_confirmed");
            };
        }
        break;
    case Qt::DisplayRole:
        switch(index.column())
        {
        case Date:
            return GUIUtil::dateTimeStr(rec->time);
        case Amount:
            return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->amount);
        case Premium:
            if (rec->premiumKnown)
                return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->premium);
            else
                return "Unknown";
        case EstimatedTimeLeft:
            return GUIUtil::blocksToTime(rec->remaining);
        case UnparkAddress:
            return QString::fromStdString(rec->unparkAddress);
        }
        break;
    case Qt::TextAlignmentRole:
        switch(index.column())
        {
        case Date:
        case UnparkAddress:
            return (int)(Qt::AlignLeft|Qt::AlignVCenter);
        case Amount:
        case EstimatedTimeLeft:
        case Premium:
            return (int)(Qt::AlignRight|Qt::AlignVCenter);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch(index.column())
        {
        case Date:
            return rec->time;
        case Amount:
            return rec->amount;
        case Premium:
            return rec->premium;
        case EstimatedTimeLeft:
            return rec->remaining;
        case UnparkAddress:
            return QString::fromStdString(rec->unparkAddress);
        }
        break;
    }
    return QVariant();
}

QVariant ParkTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
    }
    return QVariant();
}

QModelIndex ParkTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    ParkRecord *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

