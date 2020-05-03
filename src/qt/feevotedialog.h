#ifndef FEEVOTEDIALOG_H
#define FEEVOTEDIALOG_H

#include <QDialog>

namespace Ui {
class FeeVoteDialog;
}
class WalletModel;

class FeeVoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FeeVoteDialog(QWidget *parent = 0);
    ~FeeVoteDialog();

    void setModel(WalletModel *model);

    void error(const QString& message);

private:
    Ui::FeeVoteDialog *ui;

    WalletModel *model;

private slots:
    void accept();
};

#endif // FEEVOTEDIALOG_H
