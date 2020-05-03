#ifndef ASSETVOTEDIALOG_H
#define ASSETVOTEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <stdint.h>

#include "util.h"

namespace Ui
{
    class AssetVoteDialog;
}

class WalletModel;

class AssetVoteDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit AssetVoteDialog(QWidget *parent = 0);
    ~AssetVoteDialog();
    void setModel(WalletModel *model);
    void error(const QString &message);
    void addTableRow(uint32_t assetId, uint16_t confirmations, uint8_t requiredSigners, uint8_t totalSigners, uint64 maxTrade, uint64 minTrade, uint8_t nativeUnitExponent);

  private:
    Ui::AssetVoteDialog *ui;
    WalletModel *model;
    void fillAssetVoteTable();
    void addVote();
    void removeVote();

  private slots:
    void accept();
    void on_addAssetVoteButton_clicked();
    void on_removeAssetVoteButton_clicked();
};

#endif // ASSETVOTEDIALOG_H
