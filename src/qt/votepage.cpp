#include <QTimer>

#include "votepage.h"
#include "ui_votepage.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "custodianvotedialog.h"
#include "motionvotedialog.h"
#include "feevotedialog.h"
#include "reputationvotedialog.h"
#include "assetvotedialog.h"
#include "datafeeddialog.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "coinmetadata.h"

VotePage::VotePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VotePage),
    model(NULL),
    lastBestBlock(NULL)
{
    ui->setupUi(this);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(MODEL_UPDATE_DELAY);

    ui->reputationTable->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    ui->assetTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
}

VotePage::~VotePage()
{
    delete ui;
}

void VotePage::setModel(WalletModel *model)
{
    this->model = model;
}

void VotePage::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
}

void VotePage::on_custodianVote_clicked()
{
    CustodianVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_motionVote_clicked()
{
    MotionVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_feeVote_clicked()
{
    FeeVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_assetVote_clicked()
{
    AssetVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_reputationVote_clicked()
{
    ReputationVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_dataFeedButton_clicked()
{
    DataFeedDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::update()
{
    if (!model)
        return;

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain)
            return;

        if (pindexBest == lastBestBlock)
            return;

        lastBestBlock = pindexBest;

        fillCustodianTable();
        fillReputationTable();

        CSignerRewardVote signerReward = lastBestBlock->GetEffectiveSignerRewardVoteResult();
        ui->eligibleSigner->setText(QString::number(signerReward.nCount));
        ui->signerReward->setText(QString("%1 BKS").arg(BitcoinUnits::format(BitcoinUnits::BTC, signerReward.nAmount)));

        fillAssetTable();
    }
}

void VotePage::fillCustodianTable()
{
    QTableWidget* table = ui->custodianTable;
    table->setRowCount(0);
    int row = 0;
    for (CBlockIndex* pindex = lastBestBlock; pindex; pindex = pindex->pprev)
    {
        BOOST_FOREACH(const CCustodianVote& custodianVote, pindex->vElectedCustodian)
        {
            table->setRowCount(row + 1);

            QTableWidgetItem *addressItem = new QTableWidgetItem();
            addressItem->setData(Qt::DisplayRole, QString::fromStdString(custodianVote.GetAddress().ToString()));
            table->setItem(row, 0, addressItem);

            QTableWidgetItem *amountItem = new QTableWidgetItem();
            amountItem->setData(Qt::DisplayRole, BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), custodianVote.nAmount));
            amountItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
            table->setItem(row, 1, amountItem);

            QTableWidgetItem *dateItem = new QTableWidgetItem();
            dateItem->setData(Qt::DisplayRole, GUIUtil::dateTimeStr(pindex->nTime));
            table->setItem(row, 2, dateItem);

            row++;
        }
    }
    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

class ScoreWidgetItem : public QTableWidgetItem
{
public:
    bool operator <(const QTableWidgetItem &other) const
    {
        return text().toDouble() < other.text().toDouble();
    }
};

void VotePage::fillReputationTable()
{
    QTableWidget* table = ui->reputationTable;
    table->setRowCount(0);
    int row = 0;

    std::map<CBitcoinAddress, int64> mapReputation;
    if (!lastBestBlock->GetEffectiveReputation(mapReputation))
        return;

    BOOST_FOREACH(PAIRTYPE(const CBitcoinAddress, int64)& pair, mapReputation)
    {
        const CBitcoinAddress& address = pair.first;
        const int64& reputation = pair.second;
        double score = (double)reputation / 4.0;

        table->setRowCount(row + 1);

        QTableWidgetItem *addressItem = new QTableWidgetItem();
        addressItem->setData(Qt::DisplayRole, QString::fromStdString(address.ToString()));
        addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, addressItem);

        QString scoreString = QString::number(score, 'f', 1);
        ScoreWidgetItem *scoreItem = new ScoreWidgetItem();
        scoreItem->setData(Qt::DisplayRole, scoreString);
        scoreItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        scoreItem->setFlags(scoreItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 1, scoreItem);

        row++;
    }
    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

void VotePage::fillAssetTable()
{
    std::map<uint32_t, CAsset> mapAssets;
    if(!lastBestBlock->GetEffectiveAssets(mapAssets))
        return;

    QTableWidget *table = ui->assetTable;
    table->setSortingEnabled(false);
    table->setRowCount(0);
    int row = 0;

    BOOST_FOREACH(PAIRTYPE(const uint32_t, CAsset) &pair, mapAssets)
    {
        const uint32_t assetId = (uint32_t) pair.first;
        const CAsset &asset = pair.second;

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
            assetName = QString::fromStdString(AssetIdToStr(assetId));
        }
        assetNameItem->setData(Qt::DisplayRole, assetName);
        assetNameItem->setFlags(assetNameItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 1, assetNameItem);

        QTableWidgetItem *confirmationsItem = new QTableWidgetItem();
        confirmationsItem->setData(Qt::DisplayRole, asset.nNumberOfConfirmations);
        confirmationsItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        confirmationsItem->setFlags(confirmationsItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 2, confirmationsItem);

        QTableWidgetItem *requiredSignersItem = new QTableWidgetItem();
        requiredSignersItem->setData(Qt::DisplayRole, asset.nRequiredDepositSigners);
        requiredSignersItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        requiredSignersItem->setFlags(requiredSignersItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 3, requiredSignersItem);

        QTableWidgetItem *totalSignersItem = new QTableWidgetItem();
        totalSignersItem->setData(Qt::DisplayRole, asset.nTotalDepositSigners);
        totalSignersItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        totalSignersItem->setFlags(totalSignersItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 4, totalSignersItem);

        QTableWidgetItem *unitExponentItem = new QTableWidgetItem();
        unitExponentItem->setData(Qt::DisplayRole, asset.nUnitExponent);
        unitExponentItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        unitExponentItem->setFlags(unitExponentItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 5, unitExponentItem);

        QTableWidgetItem *maxTradeItem = new QTableWidgetItem();
        maxTradeItem->setData(Qt::DisplayRole, GUIUtil::unitsToCoins(asset.GetMaxTrade(), asset.nUnitExponent));
        maxTradeItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        maxTradeItem->setFlags(maxTradeItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 6, maxTradeItem);

        QTableWidgetItem *minTradeItem = new QTableWidgetItem();
        minTradeItem->setData(Qt::DisplayRole, GUIUtil::unitsToCoins(asset.GetMinTrade(), asset.nUnitExponent));
        minTradeItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        minTradeItem->setFlags(minTradeItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 7, minTradeItem);

        row++;
    }

    table->setVisible(false);
    table->setSortingEnabled(true);
    table->resizeColumnsToContents();
    table->setVisible(true);
}
