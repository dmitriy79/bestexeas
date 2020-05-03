#ifndef DATAFEEDDIALOG_H
#define DATAFEEDDIALOG_H

#include <QDialog>
#include <QMessageBox>
#include <QFuture>
#include <QFutureWatcher>
#include "datafeed.h"

namespace Ui {
class DataFeedDialog;
}
class WalletModel;

class DataFeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataFeedDialog(QWidget *parent = 0);
    ~DataFeedDialog();

    void setModel(WalletModel *model);
    void setDataFeed(const CDataFeed&);
    CDataFeed getDataFeed() const;

private:
    Ui::DataFeedDialog *ui;

    WalletModel *model;

    CDataFeed previousDataFeed;
    QMessageBox waitDialog;
    QFuture<void> updateResult;
    QFutureWatcher<void> updateWatcher;

    bool confirmAfterError(QString error);
    void error(const QString& message);

private slots:
    void accept();
    void updateFinished();
};

#endif // DATAFEEDDIALOG_H
