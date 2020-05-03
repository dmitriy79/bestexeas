#include "dividendkeysdialog.h"
#include "ui_dividendkeysdialog.h"
#include "base58.h"

DividendKeysDialog::DividendKeysDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DividendKeysDialog)
{
    ui->setupUi(this);
}

DividendKeysDialog::~DividendKeysDialog()
{
    delete ui;
}

void DividendKeysDialog::setKeys(const std::vector<CDividendSecret>& vSecret)
{
    QString text;
    for (std::vector<CDividendSecret>::const_iterator it = vSecret.begin(); it != vSecret.end(); ++it)
    {
        if (it != vSecret.begin())
            text.append("\n");
        text.append(QString::fromStdString(it->ToString()));
    }
    ui->keys->setPlainText(text);
}
