#ifndef DIVIDENDKEYSDIALOG_H
#define DIVIDENDKEYSDIALOG_H

#include <QDialog>

class CDividendSecret;

namespace Ui {
class DividendKeysDialog;
}

class DividendKeysDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DividendKeysDialog(QWidget *parent = 0);
    ~DividendKeysDialog();

    void setKeys(const std::vector<CDividendSecret>& vSecret);

private:
    Ui::DividendKeysDialog *ui;
};

#endif // DIVIDENDKEYSDIALOG_H
