#include <QMessageBox>

#include "reputationvotedialog.h"
#include "ui_reputationvotedialog.h"

#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"

using namespace std;

ReputationVoteDialog::ReputationVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReputationVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle("Reputation vote");
}

ReputationVoteDialog::~ReputationVoteDialog()
{
    delete ui;
}

void ReputationVoteDialog::setModel(WalletModel *model)
{
    this->model = model;

    CVote vote = model->getVote();

    ui->upVoteTable->setRowCount(0);
    ui->downVoteTable->setRowCount(0);
    for (int i = 0; i < vote.vReputationVote.size(); i++)
    {
        const CReputationVote& reputationVote = vote.vReputationVote[i];

        QTableWidget* table = (reputationVote.nWeight >= 0 ? ui->upVoteTable : ui->downVoteTable);
        int row = table->rowCount();
        table->setRowCount(row + 1);

        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(reputationVote.GetAddress().ToString()));
        table->setItem(row, 0, addressItem);

        QString weight = QString::number(abs(reputationVote.nWeight));
        QTableWidgetItem *weightItem = new QTableWidgetItem(weight);
        weightItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(row, 1, weightItem);
    }

    ui->upVoteTable->setVisible(false);
    ui->upVoteTable->resizeColumnsToContents();
    ui->upVoteTable->setVisible(true);

    ui->downVoteTable->setVisible(false);
    ui->downVoteTable->resizeColumnsToContents();
    ui->downVoteTable->setVisible(true);

    ui->eligibleSignerCount->setText(vote.signerReward.nCount >= 0 ? QString::number(vote.signerReward.nCount) : "");
    ui->signerReward->setText(vote.signerReward.nAmount >= 0 ? BitcoinUnits::format(BitcoinUnits::BTC, vote.signerReward.nAmount) : "");
}

void ReputationVoteDialog::addVote(QTableWidget* table)
{
    table->setRowCount(table->rowCount() + 1);
}

void ReputationVoteDialog::removeVote(QTableWidget* table)
{
    QItemSelection selection(table->selectionModel()->selection());

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
            table->removeRow(current);
            prev = current;
        }
    }
}

void ReputationVoteDialog::on_addUpVoteButton_clicked()
{
    addVote(ui->upVoteTable);
}
void ReputationVoteDialog::on_removeUpVoteButton_clicked()
{
    removeVote(ui->upVoteTable);
}
void ReputationVoteDialog::on_addDownVoteButton_clicked()
{
    addVote(ui->downVoteTable);
}
void ReputationVoteDialog::on_removeDownVoteButton_clicked()
{
    removeVote(ui->downVoteTable);
}

void ReputationVoteDialog::error(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

bool ReputationVoteDialog::parseVoteTable(QTableWidget* table, QString tableName, CVote& vote)
{
    int rows = table->rowCount();

    for (int i = 0; i < rows; i++)
    {
        int row = i + 1;
        QTableWidgetItem *addressItem = table->item(i, 0);
        QTableWidgetItem *weightItem = table->item(i, 1);
        if (!addressItem && !weightItem)
            continue;

        QString addressString = addressItem ? addressItem->text() : "";
        CBitcoinAddress address(addressString.toStdString());
        unsigned char cUnit = address.GetUnit();

        if (cUnit != '8')
        {
            error(tr("Invalid address on row %1 in %2: %3").arg(row).arg(tableName).arg(addressString));
            return false;
        }

        int weight = 0;
        if (weightItem)
            weight = weightItem->text().toInt();

        if (weight < 0 || weight > 127)
        {
            error(tr("The weight on row %1 in %2 must be between 0 and 127").arg(row).arg(tableName));
            return false;
        }

        CReputationVote reputationVote;
        reputationVote.SetAddress(address);
        if (table == ui->upVoteTable)
            reputationVote.nWeight = weight;
        else
            reputationVote.nWeight = -weight;

        vote.vReputationVote.push_back(reputationVote);
    }
    return true;
}

void ReputationVoteDialog::accept()
{
    CVote vote = model->getVote();

    vote.vReputationVote.clear();

    if (!parseVoteTable(ui->upVoteTable, tr("up vote table"), vote))
        return;
    if (!parseVoteTable(ui->downVoteTable, tr("down vote table"), vote))
        return;

    {
        QString text = ui->eligibleSignerCount->text();
        if (text == "")
            vote.signerReward.nCount = -1;
        else
        {
            int nCount = text.toInt();
            if (nCount < numeric_limits<int16_t>::min() || nCount > numeric_limits<int16_t>::max())
            {
                error(tr("Invalid eligible signer amount"));
                return;
            }
            vote.signerReward.nCount = nCount;
        }
    }

    {
        QString text = ui->signerReward->text();
        if (text == "")
            vote.signerReward.nAmount = -1;
        else
        {
            int64 nAmount;
            bool validAmount = BitcoinUnits::parse(BitcoinUnits::BTC, text, &nAmount);
            if (!validAmount || nAmount < numeric_limits<int32_t>::min() || nAmount > numeric_limits<int32_t>::max())
            {
                error(tr("Invalid signer reward amount"));
                return;
            }
            vote.signerReward.nAmount = nAmount;
        }
    }

    if (!vote.IsValid(model->getProtocolVersion()))
    {
        error(tr("The new vote is invalid"));
        return;
    }
    model->setVote(vote);
    QDialog::accept();
}
