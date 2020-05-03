#ifndef PARKDIALOG_H
#define PARKDIALOG_H

#include <QDialog>
#include <QDateTime>

namespace Ui {
class ParkDialog;
}
class WalletModel;

class ParkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParkDialog(QWidget *parent = 0);
    ~ParkDialog();

    void setModel(WalletModel *model);

    void setAddress(QString);

    qint64 getAmount() const;
    qint64 getBlocks() const;
    QString getUnparkAddress() const;
    qint64 getEstimatedPremium() const;

    QString formatAmount(qint64 amount) const;
    QString formatBlockTime(qint64 blocks) const;
    QString getUnitName() const;

private:
    Ui::ParkDialog *ui;
    WalletModel *model;

    bool confirmPark();
    void updatePremium();

private slots:
    void on_pasteButton_clicked();
    void on_addressBookButton_clicked();
    void accept();
    void on_amount_textChanged();
    void on_end_dateTimeChanged(const QDateTime& datetime);
};

#endif // PARKDIALOG_H
