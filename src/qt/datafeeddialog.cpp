#include <QMessageBox>
#include <QtConcurrentRun>

#include "datafeeddialog.h"
#include "ui_datafeeddialog.h"
#include "walletmodel.h"

DataFeedDialog::DataFeedDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataFeedDialog),
    waitDialog(this),
    updateWatcher(this)
{
    ui->setupUi(this);

    waitDialog.setWindowTitle(tr("Data feed update"));
    waitDialog.setText(tr("Fetching feed, please wait..."));
    waitDialog.setStandardButtons(QMessageBox::NoButton);
    waitDialog.setIcon(QMessageBox::Information);

    connect(&updateWatcher, SIGNAL(finished()), this, SLOT(updateFinished()));
}

DataFeedDialog::~DataFeedDialog()
{
    delete ui;
}

void DataFeedDialog::setModel(WalletModel *model)
{
    this->model = model;
    setDataFeed(model->getDataFeed());
}

void DataFeedDialog::setDataFeed(const CDataFeed& dataFeed)
{
    ui->urlEdit->setText(QString::fromStdString(dataFeed.sURL));
    ui->signatureUrlEdit->setText(QString::fromStdString(dataFeed.sSignatureURL));
    ui->signatureAddressEdit->setText(QString::fromStdString(dataFeed.sSignatureAddress));
    const std::vector<std::string>& vParts = dataFeed.vParts;
    ui->custodiansCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "custodians") != vParts.end());
    ui->motionsCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "motions") != vParts.end());
    ui->feesCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "fees") != vParts.end());
    ui->reputationsCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "reputations") != vParts.end());
    ui->signerRewardCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "signerreward") != vParts.end());
    ui->assetsCheckBox->setChecked(std::find(vParts.begin(), vParts.end(), "assets") != vParts.end());
}

CDataFeed DataFeedDialog::getDataFeed() const
{
    CDataFeed dataFeed;
    dataFeed.sURL = ui->urlEdit->text().toUtf8().constData();
    dataFeed.sSignatureURL = ui->signatureUrlEdit->text().toUtf8().constData();
    dataFeed.sSignatureAddress = ui->signatureAddressEdit->text().toUtf8().constData();
    std::vector<std::string> vParts;
    if (ui->custodiansCheckBox->isChecked())
        vParts.push_back("custodians");
    if (ui->motionsCheckBox->isChecked())
        vParts.push_back("motions");
    if (ui->feesCheckBox->isChecked())
        vParts.push_back("fees");
    if (ui->reputationsCheckBox->isChecked())
        vParts.push_back("reputations");
    if (ui->signerRewardCheckBox->isChecked())
        vParts.push_back("signerreward");
    if (ui->assetsCheckBox->isChecked())
        vParts.push_back("assets");
    dataFeed.vParts = vParts;
    return dataFeed;
}

bool DataFeedDialog::confirmAfterError(QString error)
{
    QMessageBox::StandardButton reply;

    QString sQuestion = tr("The initial download failed with the following error:\n%1\n\nDo you really want to set this URL as data feed?").arg(error);
    reply = QMessageBox::warning(this, tr("Data feed error confirmation"), sQuestion, QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void DataFeedDialog::error(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void DataFeedDialog::accept()
{
    if (ui->signatureUrlEdit->text().isEmpty() != ui->signatureAddressEdit->text().isEmpty())
    {
        error(tr("The signature URL and address fields must be both filled or both empty."));
        return;
    }

    waitDialog.setVisible(true);

    previousDataFeed = model->getDataFeed();
    model->setDataFeed(getDataFeed());

    updateResult = QtConcurrent::run(model, &WalletModel::updateFromDataFeed);
    updateWatcher.setFuture(updateResult);
}

void DataFeedDialog::updateFinished()
{
    waitDialog.setVisible(false);
    try
    {
        updateResult.waitForFinished();
        QDialog::accept();
    }
    catch (WalletModelException& e)
    {
        model->setDataFeed(previousDataFeed);
        if (confirmAfterError(e.message))
        {
            model->setDataFeed(getDataFeed());
            QDialog::accept();
        }
    }
}
