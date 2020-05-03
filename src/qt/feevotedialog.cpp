#include <QMessageBox>

#include "feevotedialog.h"
#include "ui_feevotedialog.h"

#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"

using namespace std;

FeeVoteDialog::FeeVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FeeVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle("Fee vote");
}

FeeVoteDialog::~FeeVoteDialog()
{
    delete ui;
}

void FeeVoteDialog::setModel(WalletModel *model)
{
    this->model = model;
    CVote vote = model->getVote();

    ui->table->setRowCount(0);
    ui->table->setRowCount(sAvailableUnits.size());
    for (int i = 0; i < sAvailableUnits.size(); i++)
    {
        unsigned char unit = sAvailableUnits[i];

        QTableWidgetItem *unitItem = new QTableWidgetItem(BitcoinUnits::baseName(unit));
        ui->table->setItem(i, 0, unitItem);

        map<unsigned char, uint32_t>::const_iterator it = vote.mapFeeVote.find(unit);
        QString feeString;
        if (it != vote.mapFeeVote.end())
            feeString = BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), it->second);

        QTableWidgetItem *feeItem = new QTableWidgetItem(feeString);
        ui->table->setItem(i, 1, feeItem);
    }

    ui->table->setVisible(false);
    ui->table->resizeColumnsToContents();
    ui->table->setVisible(true);
}

void FeeVoteDialog::error(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void FeeVoteDialog::accept()
{
    QTableWidget *table = ui->table;
    map<unsigned char, uint32_t> mapFee;

    for (int i = 0; i < sAvailableUnits.size(); i++)
    {
        QTableWidgetItem *feeItem = table->item(i, 1);
        if (!feeItem)
            continue;

        QString feeString(feeItem->text());
        if (feeString == "")
            continue;

        int currentUnit = model->getOptionsModel()->getDisplayUnit();
        qint64 amount = 0;
        bool validAmount = BitcoinUnits::parse(currentUnit, feeString, &amount);
        if (!validAmount || amount < 0 || amount >= numeric_limits<uint32_t>::max())
        {
            error(tr("Invalid amount: %1").arg(feeString));
            return;
        }
        mapFee[sAvailableUnits[i]] = (uint32_t)amount;
    }

    CVote vote = model->getVote();
    vote.mapFeeVote = mapFee;
    if (!vote.IsValid(model->getProtocolVersion()))
    {
        error(tr("The new vote is invalid"));
        return;
    }
    model->setVote(vote);
    QDialog::accept();
}
