#include <QMessageBox>

#include "motionvotedialog.h"
#include "ui_motionvotedialog.h"

#include "walletmodel.h"

using namespace std;

MotionVoteDialog::MotionVoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MotionVoteDialog)
{
    ui->setupUi(this);
    setWindowTitle("Motion vote");
}

MotionVoteDialog::~MotionVoteDialog()
{
    delete ui;
}

void MotionVoteDialog::setModel(WalletModel *model)
{
    this->model = model;
    CVote vote = model->getVote();

    ui->table->setRowCount(0);
    ui->table->setRowCount(vote.vMotion.size());
    for (int i = 0; i < vote.vMotion.size(); i++)
    {
        const uint160& motionVote = vote.vMotion[i];

        QTableWidgetItem *hashItem = new QTableWidgetItem(QString::fromStdString(motionVote.GetHex()));
        ui->table->setItem(i, 0, hashItem);
    }

    ui->table->setVisible(false);
    ui->table->resizeColumnsToContents();
    ui->table->setVisible(true);
}

void MotionVoteDialog::on_add_clicked()
{
    ui->table->setRowCount(ui->table->rowCount() + 1);
}

void MotionVoteDialog::on_remove_clicked()
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

void MotionVoteDialog::error(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void MotionVoteDialog::accept()
{
    QRegExp rx("^[0-9A-Fa-f]*$");

    QTableWidget *table = ui->table;
    int rows = table->rowCount();
    vector<uint160> vVote;

    for (int i = 0; i < rows; i++)
    {
        int row = i + 1;
        QTableWidgetItem *hashItem = table->item(i, 0);
        if (!hashItem)
            continue;

        QString motionHash(hashItem->text());
        if (motionHash == "")
            continue;

        if (!rx.exactMatch(motionHash))
        {
            error(tr("The motion hash must only contain hexdecimal characters (row %1)").arg(row));
            return;
        }
        vVote.push_back(uint160(motionHash.toAscii().data()));
    }

    CVote vote = model->getVote();
    vote.vMotion = vVote;
    if (!vote.IsValid(model->getProtocolVersion()))
    {
        error(tr("The new vote is invalid"));
        return;
    }
    model->setVote(vote);
    QDialog::accept();
}
