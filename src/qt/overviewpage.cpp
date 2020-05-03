#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    txdelegate(new TxViewDelegate())
{
    ui->setupUi(this);

    // Balance: <balance>
    ui->labelBalance->setFont(QFont("Monospace", -1, QFont::Bold));
    ui->labelBalance->setToolTip(tr("Your current balance"));
    ui->labelBalance->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);

    // ppcoin: stake: <stake>
    ui->labelStake->setFont(QFont("Monospace", -1, QFont::Bold));
    ui->labelStake->setToolTip(tr("Your current stake"));
    ui->labelStake->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);

    // Unconfirmed balance: <balance>
    ui->labelUnconfirmed->setFont(QFont("Monospace", -1, QFont::Bold));
    ui->labelUnconfirmed->setToolTip(tr("Total of transactions that have yet to be confirmed, and do not yet count toward the current balance"));
    ui->labelUnconfirmed->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);

    // nubit: parked: <parked>
    ui->labelParked->setFont(QFont("Monospace", -1, QFont::Bold));
    ui->labelParked->setToolTip(tr("Your current parked amount"));
    ui->labelParked->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);

    ui->labelNumTransactions->setToolTip(tr("Total number of transactions in wallet"));

    // Recent transactions
    ui->listTransactions->setStyleSheet("QListView { background:transparent }");
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setSelectionMode(QAbstractItemView::NoSelection);
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SIGNAL(transactionClicked(QModelIndex)));

    // Hiding doesn't remove the margin so we have to remove the widgets
    // See https://bugreports.qt-project.org/browse/QTBUG-6864
    QFormLayout* formLayout = (QFormLayout*)ui->frame->layout();
    formLayout->getWidgetPosition(ui->labelStake, &stakeRow, NULL);
    removeLabels(ui->labelParkedLabel, ui->labelParked);
    removeLabels(ui->labelStakeLabel, ui->labelStake);
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::removeLabels(QLabel* labelLabel, QLabel* label)
{
    labelLabel->hide();
    label->hide();
    ui->frame->layout()->removeWidget(labelLabel);
    ui->frame->layout()->removeWidget(label);
}

void OverviewPage::insertLabels(QLabel* labelLabel, QLabel* label, int row)
{
    QFormLayout* formLayout = (QFormLayout*)ui->frame->layout();

    formLayout->insertRow(row, labelLabel, label);
    labelLabel->show();
    label->show();
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 parked)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentParked = parked;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelParked->setText(BitcoinUnits::formatWithUnit(unit, parked));

    if (model->getUnit() == '8')
    {
        removeLabels(ui->labelParkedLabel, ui->labelParked);
        insertLabels(ui->labelStakeLabel, ui->labelStake, stakeRow);
    }
    else
    {
        removeLabels(ui->labelStakeLabel, ui->labelStake);
        insertLabels(ui->labelParkedLabel, ui->labelParked, stakeRow);
    }
}

void OverviewPage::setNumTransactions(int count)
{
    ui->labelNumTransactions->setText(QLocale::system().toString(count));
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model)
    {
        // Set up transaction list
        TransactionFilterProxy *filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getParked());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged()));

        emit displayUnitChanged();
    }
}

void OverviewPage::displayUnitChanged()
{
    if(!model || !model->getOptionsModel())
        return;
    if(currentBalance != -1)
        setBalance(currentBalance, currentStake, currentUnconfirmedBalance, currentParked);

    txdelegate->unit = model->getOptionsModel()->getDisplayUnit();
    ui->listTransactions->update();
}
