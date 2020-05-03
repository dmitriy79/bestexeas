#include <QClipboard>
#include <QMessageBox>
#include <QStandardItemModel>

#include "parkdialog.h"
#include "ui_parkdialog.h"

#include "addressbookpage.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "askpassphrasedialog.h"

using namespace std;

ParkDialog::ParkDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParkDialog),
    model(NULL)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->unparkAddress, this);

    QDateTime date = QDateTime::currentDateTime();
    ui->end->setDateTime(date);

    ui->amount->setFocus();
}

ParkDialog::~ParkDialog()
{
    delete ui;
}

void ParkDialog::setModel(WalletModel *model)
{
    this->model = model;

    std::vector<CParkRate> parkRates;
    {
        LOCK(cs_main);
        BOOST_FOREACH(const CParkRateVote& parkRateResult, pindexBest->vParkRateResult)
        {
            if (parkRateResult.cUnit == model->getUnit())
            {
                parkRates = parkRateResult.vParkRate;
                break;
            }
        }
    }
    ui->rates->setColumnCount(2);
    ui->rates->setRowCount(parkRates.size());
    ui->rates->setHorizontalHeaderLabels(QStringList() << tr("Duration") << tr("Annual interest rate"));
    for (int i = 0; i < parkRates.size(); i++)
    {
        const CParkRate& parkRate = parkRates[i];

        QString durationString = GUIUtil::blocksToTime(parkRate.GetDuration());
        QTableWidgetItem *durationItem = new QTableWidgetItem(durationString);
        durationItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        ui->rates->setItem(i, 0, durationItem);

        double interestRate = GUIUtil::annualInterestRatePercentage(parkRate.nRate, parkRate.GetDuration());
        QString rateString = QString("%L1%").arg(interestRate, 0, 'f', 3);
        QTableWidgetItem *rateItem = new QTableWidgetItem(rateString);
        rateItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        ui->rates->setItem(i, 1, rateItem);
    }
    ui->rates->setVisible(false);
    ui->rates->resizeColumnsToContents();
    ui->rates->setVisible(true);
}

void ParkDialog::setAddress(QString addr)
{
    ui->unparkAddress->setText(addr);
}

void ParkDialog::on_pasteButton_clicked()
{
    setAddress(QApplication::clipboard()->text());
}

void ParkDialog::on_addressBookButton_clicked()
{
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        setAddress(dlg.getReturnValue());
    }
}

qint64 ParkDialog::getAmount() const
{
    return ui->amount->value();
}

qint64 ParkDialog::getBlocks() const
{
    QDateTime end = ui->end->dateTime();
    QDateTime now = QDateTime::currentDateTime();
    return now.msecsTo(end) / 1000 / STAKE_TARGET_SPACING;
}

QString ParkDialog::getUnparkAddress() const
{
    return ui->unparkAddress->text();
}

QString ParkDialog::formatAmount(qint64 amount) const
{
    return BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), amount);
}

QString ParkDialog::formatBlockTime(qint64 blocks) const
{
    return GUIUtil::blocksToTime(blocks);
}

QString ParkDialog::getUnitName() const
{
    return BitcoinUnits::baseName();
}

bool ParkDialog::confirmPark()
{
    QMessageBox::StandardButton reply;

    QString sQuestion = tr("%1 %2 will be parked for about %3.\nThen they will be sent back to %4.\nThe estimated premium is %5 %6.\n\nAre you sure?").arg(
            formatAmount(getAmount()),
            getUnitName(),
            formatBlockTime(getBlocks()),
            getUnparkAddress(),
            formatAmount(getEstimatedPremium()),
            getUnitName());
    reply = QMessageBox::warning(this, tr("Parking confirmation"), sQuestion, QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void ParkDialog::accept()
{
    try
    {
        if (!ui->amount->validate())
            throw runtime_error(tr("Invalid amount").toStdString());

        if (getBlocks() <= 0)
            throw runtime_error(tr("Invalid end date").toStdString());

        if (!model->validateAddress(ui->unparkAddress->text()))
            throw runtime_error(tr("Invalid unpark address").toStdString());

        if (!ParkDurationRange(getBlocks()))
            throw runtime_error(tr("Invalid duration").toStdString());

        if (getEstimatedPremium() == 0)
            throw runtime_error(tr("Parking this amount for this duration would not provide any premium").toStdString());

        if (!confirmPark())
            return;

        bool fMustLock = false;
        if (model->getEncryptionStatus() == WalletModel::Locked)
        {
            AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
            dlg.setModel(model);
            dlg.exec();

            if(model->getEncryptionStatus() != WalletModel::Unlocked)
                return;

            fMustLock = true;
        }

        QString result = model->park(getAmount(), getBlocks(), getUnparkAddress());

        if (fMustLock)
            model->setWalletLocked(true);

        if (result == "")
            QDialog::accept();
        else
            throw runtime_error(result.toStdString());
    }
    catch (runtime_error &error)
    {
        QMessageBox::critical(this, tr("Error"), error.what());
    }
}

void ParkDialog::on_amount_textChanged()
{
    updatePremium();
}

void ParkDialog::on_end_dateTimeChanged(const QDateTime& datetime)
{
    updatePremium();
}

qint64 ParkDialog::getEstimatedPremium() const
{
    return model->getNextPremium(getAmount(), getBlocks());
}

void ParkDialog::updatePremium()
{
    if (!model)
        return;

    if (!ui->amount->validate())
    {
        ui->estimatedPremium->setText("");
        return;
    }

    qint64 premium = getEstimatedPremium();
    QString amountString = formatAmount(premium);
    ui->estimatedPremium->setText(QString("%1 %2").arg(amountString, getUnitName()));
}
