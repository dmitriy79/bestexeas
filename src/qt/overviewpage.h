#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QLabel>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class WalletModel;
class TxViewDelegate;

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setModel(WalletModel *model);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 parked);
    void setNumTransactions(int count);

signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    WalletModel *model;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentParked;

    TxViewDelegate *txdelegate;

    int stakeRow;

    void removeLabels(QLabel* labelLabel, QLabel* label);
    void insertLabels(QLabel* labelLabel, QLabel* label, int row);

private slots:
    void displayUnitChanged();
};

#endif // OVERVIEWPAGE_H
