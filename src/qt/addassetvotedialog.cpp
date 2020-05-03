#include <QMessageBox>

#include "addassetvotedialog.h"
#include "ui_addassetvotedialog.h"
#include "assetvotedialog.h"
#include "coinmetadata.h"
#include "exchange.h"
#include "guiutil.h"
#include "walletmodel.h"

using namespace std;

#define DEFAULT_CONFIRMATIONS (6)
#define DEFAULT_REQ_SIGNERS (2)
#define DEFAULT_TOTAL_SIGNERS (3)
#define DEFAULT_MAX_TRADE (100000000)
#define DEFAULT_MIN_TRADE (100000)
#define DEFAULT_UNIT_EXPONENT (8)

AddAssetVoteDialog::AddAssetVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddAssetVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Add asset vote"));
}

AddAssetVoteDialog::~AddAssetVoteDialog()
{
    delete ui;
}

void AddAssetVoteDialog::updateFields()
{
    QComboBox *assetList = ui->assetList;
    int index = assetList->currentIndex();

    if(index == assetList->count() - 1)
    {
        // Custom asset
        ui->assetId->clear();
        ui->assetId->setReadOnly(false);
        ui->assetId->setEnabled(true);
        ui->unitExponent->setText(QString::number(DEFAULT_UNIT_EXPONENT));
        ui->unitExponent->setReadOnly(false);
        ui->unitExponent->setEnabled(true);
        ui->assetSymbol1->clear();
        ui->assetSymbol2->clear();
    }
    else
    {
        uint32_t assetId = assetList->itemData(index).toUInt();
        QString assetSymbol = QString::fromStdString(GetAssetSymbol(assetId));

        ui->assetId->setText(QString::fromStdString(AssetIdToStr(assetId)));
        ui->assetId->setReadOnly(true);
        ui->assetId->setEnabled(false);
        ui->unitExponent->setText(QString::number(GetAssetUnitExponent(assetId)));
        ui->unitExponent->setReadOnly(true);
        ui->unitExponent->setEnabled(false);
        ui->assetSymbol1->setText(assetSymbol);
        ui->assetSymbol2->setText(assetSymbol);
    }
}

void AddAssetVoteDialog::setModel(WalletModel *model)
{
    this->model = model;

    QComboBox *assetList = ui->assetList;
    assetList->clear();
    for(coin_metadata_map::const_iterator it = COIN_METADATA.begin(); it != COIN_METADATA.end(); it++)
    {
        uint32_t assetId = it->first;
        QString assetName = QString::fromStdString(GetAssetName(assetId));
        QString assetSymbol = QString::fromStdString(GetAssetSymbol(assetId));
        assetList->addItem(assetName + " (" + assetSymbol + ")", assetId);
    }
    assetList->addItem(tr("other..."));

    ui->confirmations->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->requiredDepositSigners->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->totalDepositSigners->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->maxTrade->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->minTrade->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->unitExponent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    ui->confirmations->setText(QString::number(DEFAULT_CONFIRMATIONS));
    ui->requiredDepositSigners->setText(QString::number(DEFAULT_REQ_SIGNERS));
    ui->totalDepositSigners->setText(QString::number(DEFAULT_TOTAL_SIGNERS));
    ui->unitExponent->setText(QString::number(DEFAULT_UNIT_EXPONENT));
    ui->maxTrade->setText(GUIUtil::unitsToCoins(DEFAULT_MAX_TRADE, DEFAULT_UNIT_EXPONENT));
    ui->minTrade->setText(GUIUtil::unitsToCoins(DEFAULT_MIN_TRADE, DEFAULT_UNIT_EXPONENT));

    updateFields();
}

void AddAssetVoteDialog::error(const QString &message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void AddAssetVoteDialog::accept()
{
    bool ok;

    uint32_t nAssetId = EncodeAssetId(ui->assetId->text().toStdString());
    if(!IsValidAssetId(nAssetId))
    {
        error(tr("Invalid asset id"));
        return;
    }

    uint16_t nNumberOfConfirmations = ui->confirmations->text().toUInt(&ok);
    if(!ok || (nNumberOfConfirmations == 0))
    {
        error(tr("Invalid number of confirmations"));
        return;
    }

    uint8_t nRequiredDepositSigners = ui->requiredDepositSigners->text().toUInt(&ok);
    if(!ok || (nRequiredDepositSigners < MIN_REQ_SIGNERS))
    {
        error(tr("Invalid number of required deposit signers"));
        return;
    }

    uint8_t nTotalDepositSigners = ui->totalDepositSigners->text().toUInt(&ok);
    if(!ok || (nTotalDepositSigners < MIN_TOTAL_SIGNERS) || (nTotalDepositSigners < nRequiredDepositSigners))
    {
        error(tr("Invalid number of total deposit signers"));
        return;
    }

    uint8_t nUnitExponent = ui->unitExponent->text().toUInt();
    if(!ok || (nUnitExponent > MAX_TRADABLE_UNIT_EXPONENT))
    {
        error(tr("Invalid unit exponent"));
        return;
    }

    uint64 nMaxTrade = GUIUtil::coinsToUnits(ui->maxTrade->text(), nUnitExponent);
    if(nMaxTrade == 0)
    {
        error(tr("Invalid max trade"));
        return;
    }

    uint64 nMinTrade = GUIUtil::coinsToUnits(ui->minTrade->text(), nUnitExponent);
    if(nMinTrade > nMaxTrade)
    {
        error(tr("Invalid min trade"));
        return;
    }

    ((AssetVoteDialog *) parentWidget())->addTableRow(nAssetId,
                                                      nNumberOfConfirmations,
                                                      nRequiredDepositSigners,
                                                      nTotalDepositSigners,
                                                      nMaxTrade,
                                                      nMinTrade,
                                                      nUnitExponent);

    QDialog::accept();
}

void AddAssetVoteDialog::on_assetList_currentIndexChanged(int index)
{
    updateFields();
}

void AddAssetVoteDialog::on_assetId_textChanged(const QString &assetId)
{
    int index = ui->assetList->currentIndex();

    if(index == ui->assetList->count() - 1)
    {
        // Custom asset
        if(IsValidAssetId(EncodeAssetId(assetId.toStdString())))
        {
            ui->assetSymbol1->setText(assetId);
            ui->assetSymbol2->setText(assetId);
        }
        else
        {
            ui->assetSymbol1->setText("");
            ui->assetSymbol2->setText("");
        }
    }
}
