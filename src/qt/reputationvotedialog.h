#ifndef REPUTATIONVOTEDIALOG_H
#define REPUTATIONVOTEDIALOG_H

#include <QDialog>
#include <QTableWidget>

namespace Ui {
class ReputationVoteDialog;
}
class WalletModel;
class CVote;

class ReputationVoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReputationVoteDialog(QWidget *parent = 0);
    ~ReputationVoteDialog();

    void setModel(WalletModel *model);

    void error(const QString& message);

private:
    Ui::ReputationVoteDialog *ui;

    WalletModel *model;

    void addVote(QTableWidget* table);
    void removeVote(QTableWidget* table);
    bool parseVoteTable(QTableWidget* table, QString tableName, CVote& vote);

private slots:
    void accept();
    void on_addUpVoteButton_clicked();
    void on_removeUpVoteButton_clicked();
    void on_addDownVoteButton_clicked();
    void on_removeDownVoteButton_clicked();
};

#endif // REPUTATIONVOTEDIALOG_H
