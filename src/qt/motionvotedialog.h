#ifndef MOTIONVOTEDIALOG_H
#define MOTIONVOTEDIALOG_H

#include <QDialog>

namespace Ui {
class MotionVoteDialog;
}
class WalletModel;

class MotionVoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MotionVoteDialog(QWidget *parent = 0);
    ~MotionVoteDialog();

    void setModel(WalletModel *model);

    void error(const QString& message);

private:
    Ui::MotionVoteDialog *ui;

    WalletModel *model;

private slots:
    void on_add_clicked();
    void on_remove_clicked();
    void accept();
};

#endif // MOTIONVOTEDIALOG_H
