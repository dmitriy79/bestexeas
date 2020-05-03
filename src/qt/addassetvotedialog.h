#ifndef ADDASSETVOTEDIALOG_H
#define ADDASSETVOTEDIALOG_H

#include <QDialog>

namespace Ui
{
    class AddAssetVoteDialog;
}

class WalletModel;

class AddAssetVoteDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit AddAssetVoteDialog(QWidget *parent = 0);
    ~AddAssetVoteDialog();
    void setModel(WalletModel *model);
    void error(const QString &message);

  private:
    Ui::AddAssetVoteDialog *ui;
    WalletModel *model;
    void updateFields();

  private slots:
    void accept();
    void on_assetList_currentIndexChanged(int index);
    void on_assetId_textChanged(const QString &assetId);
};

#endif //ADDASSETVOTEDIALOG_H
