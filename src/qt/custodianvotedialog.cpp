#include <QMessageBox>

#include "custodianvotedialog.h"
#include "ui_custodianvotedialog.h"
#include "walletmodel.h"
#include "vote.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"

using namespace std;

CustodianVoteDialog::CustodianVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CustodianVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle("Custodian vote");
}

CustodianVoteDialog::~CustodianVoteDialog()
{
    delete ui;
}

void CustodianVoteDialog::setModel(WalletModel *model)
{
    this->model = model;
    CVote vote = model->getVote();

    ui->table->setRowCount(0);
    ui->table->setRowCount(vote.vCustodianVote.size());
    for (int i = 0; i < vote.vCustodianVote.size(); i++)
    {
        const CCustodianVote& custodianVote = vote.vCustodianVote[i];

        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(custodianVote.GetAddress().ToString()));
        ui->table->setItem(i, 0, addressItem);

        QString amount = BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), custodianVote.nAmount);
        QTableWidgetItem *amountItem = new QTableWidgetItem(amount);
        amountItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        ui->table->setItem(i, 1, amountItem);
    }

    ui->table->setVisible(false);
    ui->table->resizeColumnsToContents();
    ui->table->setVisible(true);
}

void CustodianVoteDialog::on_add_clicked()
{
    ui->table->setRowCount(ui->table->rowCount() + 1);
}

void CustodianVoteDialog::on_remove_clicked()
{
    QItemSelection selection(ui->table->selectionModel()->selection());

    QList<int> rows;
    foreach(const QModelIndex& index, selection.indexes())
        rows.append(index.row());

    qSort(rows);

    int prev = -1;
    for (int i = rows.count() - 1; i >= 0; i -= 1)
    {
        int current = rows[i];
        if (current != prev)
        {
            ui->table->removeRow(current);
            prev = current;
        }
    }
}

void CustodianVoteDialog::error(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void CustodianVoteDialog::accept()
{
    QTableWidget *table = ui->table;
    int rows = table->rowCount();
    vector<CCustodianVote> vVote;

    for (int i = 0; i < rows; i++)
    {
        int row = i + 1;
        QTableWidgetItem *addressItem = table->item(i, 0);
        QTableWidgetItem *amountItem = table->item(i, 1);
        if (!addressItem && !amountItem)
            continue;

        QString addressString = addressItem ? addressItem->text() : "";
        CBitcoinAddress address(addressString.toStdString());
        unsigned char cUnit = address.GetUnit();

        if (!IsValidUnit(cUnit) || (pindexBest->nProtocolVersion < PROTOCOL_V2_0 && !IsValidCurrency(cUnit)) || !address.IsValid(cUnit))
        {
            error(tr("Invalid address on row %1: %2").arg(row).arg(addressString));
            return;
        }

        qint64 amount = 0;
        if (amountItem)
        {
            int currentUnit = model->getOptionsModel()->getDisplayUnit();
            bool validAmount = BitcoinUnits::parse(currentUnit, amountItem->text(), &amount);
            if (!validAmount)
                amount = 0;
        }
        if (amount <= 0)
        {
            error(tr("Invalid amount on row %1").arg(row));
            return;
        }

        CCustodianVote vote;
        vote.SetAddress(address);
        vote.nAmount = amount;

        vVote.push_back(vote);
    }

    CVote vote = model->getVote();
    vote.vCustodianVote = vVote;
    if (!vote.IsValid(model->getProtocolVersion()))
    {
        error(tr("The new vote is invalid"));
        return;
    }
    model->setVote(vote);
    QDialog::accept();
}
