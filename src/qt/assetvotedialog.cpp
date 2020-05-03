#include <QMessageBox>
#include <stdint.h>

#include "assetvotedialog.h"
#include "ui_assetvotedialog.h"
#include "addassetvotedialog.h"
#include "coinmetadata.h"
#include "guiutil.h"
#include "util.h"
#include "vote.h"
#include "walletmodel.h"

using namespace std;

AssetVoteDialog::AssetVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AssetVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Asset vote"));
    ui->assetVoteTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
}

AssetVoteDialog::~AssetVoteDialog()
{
    delete ui;
}

void AssetVoteDialog::setModel(WalletModel *model)
{
    this->model = model;
    fillAssetVoteTable();
}

void AssetVoteDialog::addTableRow(uint32_t assetId,
                                  uint16_t confirmations,
                                  uint8_t requiredSigners,
                                  uint8_t totalSigners,
                                  uint64 maxTrade,
                                  uint64 minTrade,
                                  uint8_t unitExponent)
{
    QTableWidget *table = ui->assetVoteTable;
    int row = table->rowCount();
    bool preDefinedAsset = true;

    table->setSortingEnabled(false);
    table->setRowCount(row + 1);

    QTableWidgetItem *idItem = new QTableWidgetItem();
    idItem->setData(Qt::DisplayRole, QString::fromStdString(AssetIdToStr(assetId)));
    idItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, idItem);

    QTableWidgetItem *assetNameItem = new QTableWidgetItem();
    QString assetName = QString::fromStdString(GetAssetName(assetId));
    if(assetName.isEmpty())
    {
        preDefinedAsset = false;
        assetName = QString::fromStdString(AssetIdToStr(assetId));
    }
    assetNameItem->setData(Qt::DisplayRole, assetName);
    assetNameItem->setFlags(assetNameItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 1, assetNameItem);

    QTableWidgetItem *confirmationsItem = new QTableWidgetItem();
    confirmationsItem->setData(Qt::DisplayRole, confirmations);
    confirmationsItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    table->setItem(row, 2, confirmationsItem);

    QTableWidgetItem *requiredSignersItem = new QTableWidgetItem();
    requiredSignersItem->setData(Qt::DisplayRole, requiredSigners);
    requiredSignersItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    table->setItem(row, 3, requiredSignersItem);

    QTableWidgetItem *totalSignersItem = new QTableWidgetItem();
    totalSignersItem->setData(Qt::DisplayRole, totalSigners);
    totalSignersItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    table->setItem(row, 4, totalSignersItem);

    QTableWidgetItem *unitExponentItem = new QTableWidgetItem();
    unitExponentItem->setData(Qt::DisplayRole, unitExponent);
    unitExponentItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    if(preDefinedAsset)
    {
        unitExponentItem->setFlags(unitExponentItem->flags() & ~Qt::ItemIsEditable);
    }
    table->setItem(row, 5, unitExponentItem);

    QTableWidgetItem *maxTradeItem = new QTableWidgetItem();
    maxTradeItem->setData(Qt::DisplayRole, GUIUtil::unitsToCoins(maxTrade, unitExponent));
    maxTradeItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    table->setItem(row, 6, maxTradeItem);

    QTableWidgetItem *minTradeItem = new QTableWidgetItem();
    minTradeItem->setData(Qt::DisplayRole, GUIUtil::unitsToCoins(minTrade, unitExponent));
    minTradeItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
    table->setItem(row, 7, minTradeItem);

    table->setSortingEnabled(true);
}

void AssetVoteDialog::fillAssetVoteTable()
{
    CVote vote = model->getVote();
    QTableWidget *table = ui->assetVoteTable;
    table->setRowCount(0);

    for(int i = 0; i < vote.vAssetVote.size(); i++)
    {
        const CAssetVote &assetVote = vote.vAssetVote[i];

        addTableRow(assetVote.nAssetId,
                    assetVote.nNumberOfConfirmations,
                    assetVote.nRequiredDepositSigners,
                    assetVote.nTotalDepositSigners,
                    assetVote.GetMaxTrade(),
                    assetVote.GetMinTrade(),
                    assetVote.nUnitExponent);
    }

    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

void AssetVoteDialog::addVote()
{
    AddAssetVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();

    QTableWidget *table = ui->assetVoteTable;
    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

void AssetVoteDialog::removeVote()
{
    QTableWidget *table = ui->assetVoteTable;
    QItemSelection selection(table->selectionModel()->selection());

    QList<int> rows;
    foreach(const QModelIndex &index, selection.indexes())
    {
        rows.append(index.row());
    }
    qSort(rows);

    int prev = -1;
    for(int i = rows.count() - 1; i >= 0; i--)
    {
        int current = rows[i];
        if(current != prev)
        {
            table->removeRow(current);
            prev = current;
        }
    }
}

void AssetVoteDialog::on_addAssetVoteButton_clicked()
{
    addVote();
}

void AssetVoteDialog::on_removeAssetVoteButton_clicked()
{
    removeVote();
}

void AssetVoteDialog::error(const QString &message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void AssetVoteDialog::accept()
{
    CVote vote = model->getVote();
    vector<CAssetVote> vAssetVote;
    QTableWidget *table = ui->assetVoteTable;
    int rows = table->rowCount();

    for(int i = 0; i < rows; i++)
    {
        CAssetVote assetVote;
        int row = i + 1;
        QTableWidgetItem *idItem = table->item(i, 0);
        QTableWidgetItem *confirmationsItem = table->item(i, 2);
        QTableWidgetItem *requiredSignersItem = table->item(i, 3);
        QTableWidgetItem *totalSignersItem = table->item(i, 4);
        QTableWidgetItem *unitExponentItem = table->item(i, 5);
        QTableWidgetItem *maxTradeItem = table->item(i, 6);
        QTableWidgetItem *minTradeItem = table->item(i, 7);

        if(!idItem && !confirmationsItem && !requiredSignersItem && !totalSignersItem && !unitExponentItem && !maxTradeItem && !minTradeItem)
            continue;

        bool ok;

        uint32_t nAssetId = EncodeAssetId(idItem->text().toStdString());
        if(!IsValidAssetId(nAssetId))
        {
            error(tr("Invalid asset ID on row %1").arg(row));
            return;
        }
        assetVote.nAssetId = nAssetId;

        uint16_t nNumberOfConfirmations = confirmationsItem->text().toUInt(&ok);
        if(!ok || (nNumberOfConfirmations == 0))
        {
            error(tr("Invalid number of confirmations on row %1").arg(row));
            return;
        }
        assetVote.nNumberOfConfirmations = nNumberOfConfirmations;

        uint8_t nRequiredDepositSigners = requiredSignersItem->text().toUInt(&ok);
        if(!ok || (nRequiredDepositSigners < MIN_REQ_SIGNERS))
        {
            error(tr("Invalid number of required deposit signers on row %1").arg(row));
            return;
        }
        assetVote.nRequiredDepositSigners = nRequiredDepositSigners;

        uint8_t nTotalDepositSigners = totalSignersItem->text().toUInt(&ok);
        if(!ok || (nTotalDepositSigners < MIN_TOTAL_SIGNERS) || (nTotalDepositSigners < nRequiredDepositSigners))
        {
            error(tr("Invalid number of total deposit signers on row %1").arg(row));
            return;
        }
        assetVote.nTotalDepositSigners = nTotalDepositSigners;

        uint8_t nUnitExponent = unitExponentItem->text().toUInt(&ok);
        if(!ok || (nUnitExponent > MAX_TRADABLE_UNIT_EXPONENT))
        {
            error(tr("Invalid unit exponent on row %1").arg(row));
            return;
        }
        assetVote.nUnitExponent = nUnitExponent;

        uint64 nMaxTrade = GUIUtil::coinsToUnits(maxTradeItem->text(), nUnitExponent);
        uint8_t nMaxTradeExpParam = ExponentialParameter(nMaxTrade);
        if((nMaxTradeExpParam == 0) || (nMaxTradeExpParam > EXP_SERIES_MAX_PARAM))
        {
            error(tr("Invalid max trade on row %1").arg(row));
            return;
        }
        assetVote.nMaxTradeExpParam = nMaxTradeExpParam;

        uint64 nMinTrade = GUIUtil::coinsToUnits(minTradeItem->text(), nUnitExponent);
        uint8_t nMinTradeExpParam = ExponentialParameter(nMinTrade);
        if((nMinTradeExpParam > nMaxTradeExpParam) || (nMinTradeExpParam > EXP_SERIES_MAX_PARAM))
        {
            error(tr("Invalid min trade on row %1").arg(row));
            return;
        }
        assetVote.nMinTradeExpParam = nMinTradeExpParam;

        vAssetVote.push_back(assetVote);
    }

    vote.vAssetVote = vAssetVote;

    if(!vote.IsValid(model->getProtocolVersion()))
    {
        error(tr("The new vote is invalid"));
        return;
    }

    model->setVote(vote);

    QDialog::accept();
}
