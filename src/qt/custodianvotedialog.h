#ifndef CUSTODIANVOTEDIALOG_H
#define CUSTODIANVOTEDIALOG_H

#include <QDialog>

namespace Ui {
class CustodianVoteDialog;
}
class WalletModel;

class CustodianVoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustodianVoteDialog(QWidget *parent = 0);
    ~CustodianVoteDialog();

    void setModel(WalletModel *model);

private:
    Ui::CustodianVoteDialog *ui;
    WalletModel *model;

    void error(const QString& message);

private slots:
    void on_add_clicked();
    void on_remove_clicked();
    void accept();
};

#endif // CUSTODIANVOTEDIALOG_H
