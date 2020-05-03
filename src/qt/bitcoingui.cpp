/*
 * Qt4 Peershares GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 * The PPCoin developers 2011-2013
 * The Peershares Developers 2013-2014
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "distributedivdialog.h"
#include "votepage.h"
#include "dividendkeysdialog.h"

#ifdef Q_WS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QSignalMapper>
#include <QInputDialog>

#include <QDragEnterEvent>
#include <QUrl>

#include <iostream>

#include <QFont>
#include <QStyleFactory>

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    exportDividendKeysAction(0),
    displayDividendKeysAction(0),
    encryptWalletAction(0),
    unlockForMintingAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    resize(850, 550);
    setWindowTitle(tr("B&C Exchange"));
    setStyle(QStyleFactory::create("cleanlooks"));
#ifndef Q_WS_MAC
    setWindowIcon(QIcon(":icons/icon"));
#else
    // nu: setting this breaks the visual styles for the toolbar, so turning off
    setUnifiedTitleAndToolBarOnMac(false);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    messagePage = new SignVerifyMessageDialog(this);

    votePage = new VotePage(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
#ifdef FIRST_CLASS_MESSAGING
    centralWidget->addWidget(messagePage);
#endif
    centralWidget->addWidget(votePage);
    setCentralWidget(centralWidget);

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setMinimumWidth(56);
    frameBlocks->setMaximumWidth(56);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBarLabel->setProperty("class", "progressBarLabel");
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);
    progressBar->setProperty("class", "statusBarStyle");

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));

    // Doubleclicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    gotoOverviewPage();
}

BitcoinGUI::~BitcoinGUI()
{
#ifdef Q_WS_MAC
    delete appMenuBar;
#endif
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of holdings"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    historyAction = new QAction(tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

    receiveCoinsAction = new QAction(tr("&Receive"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving %1").arg(BitcoinUnits::baseName()));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    sendCoinsAction = new QAction(tr("&Send"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a %1 address").arg(BitcoinUnits::baseName()));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    messageAction = new QAction(QIcon(":/icons/edit"), tr("Sign/Verify &message"), this);
    messageAction->setToolTip(tr("Prove you control an address"));
#ifdef FIRST_CLASS_MESSAGING
    messageAction->setCheckable(true);
#endif
    tabGroup->addAction(messageAction);

    voteAction = new QAction(tr("&Vote"), this);
    voteAction->setToolTip(tr("Change your vote"));
    voteAction->setCheckable(true);
    voteAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    tabGroup->addAction(voteAction);

    switchUnitAction = new QAction(tr(""), this);
    switchUnitAction->setToolTip(tr("Switch unit"));
    switchUnitAction->setCheckable(true);
    switchUnitAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
    tabGroup->addAction(switchUnitAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(messageAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(messageAction, SIGNAL(triggered()), this, SLOT(gotoMessagePage()));
    connect(voteAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(voteAction, SIGNAL(triggered()), this, SLOT(gotoVotePage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/ppcoin"), tr("&About %1").arg(qApp->applicationName().replace("&", "&&")), this);
    aboutAction->setToolTip(tr("Show information about B&C Exchange"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for B&C Exchange"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/ppcoin"), tr("Show/Hide &B\\&C Exchange"), this);
    toggleHideAction->setToolTip(tr("Show or hide the B&C Exchange window"));
    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Portfolio"), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt portfolio"));
    encryptWalletAction->setCheckable(true);
    unlockForMintingAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Unlock Wallet for Minting Only"), this);
    unlockForMintingAction->setToolTip(tr("Unlock wallet only for minting. Sending BlockShares will still require the passphrase."));
    unlockForMintingAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet"), this);
    backupWalletAction->setToolTip(tr("Backup portfolio to another location"));
    importAction = new QAction(QIcon(":/icons/import_nsr_wallet"), tr("&Import NuShares Wallet..."), this);
    importAction->setToolTip(tr("Import an NSR wallet file."));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase"), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for portfolio encryption"));
    openRPCConsoleAction = new QAction(tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));
    exportDividendKeysAction = new QAction(QIcon(":/icons/export"), tr("&Export dividend keys..."), this);
    exportDividendKeysAction->setToolTip(tr("Export the dividend keys associated with the BlockShares addresses to Bitcoin via RPC"));
    displayDividendKeysAction = new QAction(QIcon(":/icons/display"), tr("D&isplay dividend keys..."), this);
    displayDividendKeysAction->setToolTip(tr("Display the dividend keys associated with the BlockShares addresses"));
    distributeDividendsAction = new QAction(tr("&Distribute dividends..."), this);
    distributeDividendsAction->setToolTip(tr("Distribute dividends to share holders"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(unlockForMintingAction, SIGNAL(triggered(bool)), this, SLOT(unlockForMinting(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(importAction, SIGNAL(triggered()), this, SLOT(walletImport()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(exportDividendKeysAction, SIGNAL(triggered()), this, SLOT(exportDividendKeys()));
    connect(displayDividendKeysAction, SIGNAL(triggered()), this, SLOT(displayDividendKeys()));
    connect(distributeDividendsAction, SIGNAL(triggered()), this, SLOT(distributeDividendsClicked()));
    connect(switchUnitAction, SIGNAL(triggered()), this, SLOT (switchUnitButtonClicked()));
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_WS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
#ifndef FIRST_CLASS_MESSAGING
    file->addAction(messageAction);
#endif
    file->addAction(importAction);
    file->addSeparator();
    file->addAction(quitAction);

    sharesMenu = appMenuBar->addMenu(tr("Sh&ares"));
    sharesMenu->addAction(exportDividendKeysAction);
    sharesMenu->addAction(displayDividendKeysAction);
    sharesMenu->addAction(distributeDividendsAction);

    unitMenu = appMenuBar->addMenu(tr("&Unit"));

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(unlockForMintingAction);
    settings->addAction(changePassphraseAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars()
{
    // spacer for the left-hand side of the toolbar frame used to display the
    // selected unit's icon and logo text
    QFrame* toolbarSpacer = new QFrame();
    toolbarSpacer->setMinimumWidth(125);
    toolbarSpacer->setProperty("class", "toolbarSpacer");

    // toolbar
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setStyle(QStyleFactory::create("cleanlooks"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->setMovable(false);
    toolbar->addWidget(toolbarSpacer);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
#ifdef FIRST_CLASS_MESSAGING
    toolbar->addAction(messageAction);
#endif
    toolbar->addAction(voteAction);
    toolbar->addAction(switchUnitAction);

    QWidget *switchUnitToggleBtn = toolbar->widgetForAction(switchUnitAction);
    switchUnitToggleBtn->setProperty("class", "switchUnitToggleBtn");
}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        if(clientModel->isTestNet())
        {
            QString title_testnet = windowTitle() + QString(" ") + tr("[testnet]");
            setWindowTitle(title_testnet);
#ifndef Q_WS_MAC
            setWindowIcon(QIcon(":icons/icon_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/icon_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(title_testnet);
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
            }
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString, bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        votePage->setClientModel(clientModel);
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // nubit: set current base unit
        BitcoinUnits::baseUnit = walletModel->getUnit();

        // nubit: update the send and receive tooltip text to display the proper unit (BlockShares/BlockCredits)
        receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving %1").arg(BitcoinUnits::baseName()));
        sendCoinsAction->setToolTip(tr("Send coins to a %1 address").arg(BitcoinUnits::baseName()));

        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        messagePage->setModel(walletModel);
        rpcConsole->setModel(walletModel);
        votePage->setModel(walletModel);

        voteAction->setVisible(walletModel->getUnit() == '8');

        importAction->setEnabled(walletModel->getUnit() == '8');

        if (walletModel->getUnit() != '8' && centralWidget->currentWidget() == votePage)
            gotoOverviewPage();

        sharesMenu->setEnabled(walletModel->getUnit() == '8');

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon popup for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // nubit: change the client stylesheet when the unit context changes
        if (walletModel->getUnit() == '8')
        {
            QFile stylesheet(":/styles/nushares.qss");
            stylesheet.open(QFile::ReadOnly);
            QString setSheet = QLatin1String(stylesheet.readAll());
            QWidget::setStyleSheet(setSheet);
        }
        else
        {
            QFile stylesheet(":/styles/nubits.qss");
            stylesheet.open(QFile::ReadOnly);
            QString setSheet = QLatin1String(stylesheet.readAll());
            QWidget::setStyleSheet(setSheet);
        }

        // Embed application fonts
        QFont newFont(":/fonts/Roboto-Regular.ttf", 14, true);
        setFont(newFont);

        changeUnitActions.clear();
        QSignalMapper *mapper = new QSignalMapper();

        BOOST_FOREACH(CWallet *wallet, setpwalletRegistered)
        {
            QString unitString(wallet->Unit());
            QAction *action = new QAction(BitcoinUnits::baseName(wallet->Unit()), this);
            action->setCheckable(true);
            if (unitString == QString(walletModel->getWallet()->Unit()))
                action->setChecked(true);

            changeUnitActions.append(action);

            connect(action, SIGNAL(triggered()), mapper, SLOT(map()));
            mapper->setMapping(action, QString(wallet->Unit()));
        }
        connect(mapper, SIGNAL(mapped(const QString&)), this, SLOT(changeUnit(const QString&)));
        unitMenu->clear();
        for (int i=0; i < changeUnitActions.size(); ++i)
            unitMenu->addAction(changeUnitActions[i]);

        if (walletModel->getUnit() == '8')
        {
            switchUnitTarget = "C";
            switchUnitAction->setText(tr("BlockCredits"));
        }
        else
        {
            switchUnitTarget = "8";
            switchUnitAction->setText(tr("BlockShares"));
        }
    }
}

void BitcoinGUI::changeUnit(const QString &unit)
{
    WalletModel *oldWalletModel = this->walletModel;
    OptionsModel *optionsModel = oldWalletModel->getOptionsModel();
    WalletModel *newWalletModel = NULL;

    BOOST_FOREACH(CWallet *wallet, setpwalletRegistered)
    {
        if (QString(wallet->Unit()) == unit)
        {
            newWalletModel = new WalletModel(wallet, optionsModel);
            break;
        }
    }
    setUpdatesEnabled(false);
    setWalletModel(newWalletModel);
    setUpdatesEnabled(true);
    delete oldWalletModel;
}

void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_WS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("B&C Exchange client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    trayIconMenu = dockIconHandler->dockMenu();
    dockIconHandler->setMainWindow((QMainWindow *)this);
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(messageAction);
#ifndef FIRST_CLASS_MESSAGING
    trayIconMenu->addSeparator();
#endif
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
#ifndef Q_WS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(tr("B&C Exchange-qt"), trayIcon);
}

#ifndef Q_WS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers "show/hide Peershares"
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::toggleHidden()
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else
        hide();
}

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to B&C Exchange network", "", count));
}

void BitcoinGUI::setNumBlocks(int count)
{
    // don't show / hide progressBar and it's label if we have no connection(s) to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    int nTotalBlocks = clientModel->getNumBlocksOfPeers();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (clientModel->getStatusBarWarnings() == "")
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }
        else
        {
            progressBarLabel->setText(clientModel->getStatusBarWarnings());
            progressBarLabel->setVisible(true);
            progressBar->setVisible(false);
        }
        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (clientModel->getStatusBarWarnings() == "")
            progressBarLabel->setVisible(false);
        else
        {
            progressBarLabel->setText(clientModel->getStatusBarWarnings());
            progressBarLabel->setVisible(true);
        }
        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    QDateTime now = QDateTime::currentDateTime();
    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(now);
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".\n") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    }
    else
    {
        tooltip = tr("Catching up...") + QString("\n") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();
    }

    if(!text.isEmpty())
    {
        tooltip += QString("\n");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_WS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_WS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction requires a network fee of %1. "
          "Do you want to pay the fee and continue?").arg(
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Sending..."), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                            TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("Sent transaction") :
                                         tr("Incoming transaction"),
                              tr("Date: %1\n"
                                 "Amount: %2\n"
                                 "Type: %3\n"
                                 "Address: %4\n")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoMessagePage()
{
#ifdef FIRST_CLASS_MESSAGING
    messageAction->setChecked(true);
    centralWidget->setCurrentWidget(messagePage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
#else
    messagePage->show();
    messagePage->setFocus();
#endif
}

void BitcoinGUI::gotoVotePage()
{
    voteAction->setChecked(true);
    centralWidget->setCurrentWidget(votePage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        gotoSendCoinsPage();
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            sendCoinsPage->handleURI(uri.toString());
        }
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    gotoSendCoinsPage();
    sendCoinsPage->handleURI(strURI);

    if(!isActiveWindow())
        activateWindow();

    showNormalIfMinimized();
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    bool fShares = (walletModel->getUnit() == '8');
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        unlockForMintingAction->setEnabled(false);
        unlockForMintingAction->setChecked(false);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(walletModel->isUnlockedForMintingOnly()? tr("Wallet is <b>encrypted</b> and currently <b>unlocked for block minting only</b>") : tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        unlockForMintingAction->setEnabled(fShares && walletModel->isUnlockedForMintingOnly());
        unlockForMintingAction->setChecked(fShares && walletModel->isUnlockedForMintingOnly());
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        unlockForMintingAction->setEnabled(fShares);
        unlockForMintingAction->setChecked(false);
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::unlockForMinting(bool status)
{
    if(!walletModel)
        return;

    if (status)
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Locked)
            return;

        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        walletModel->setUnlockedForMintingOnly(true);
    }
    else
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        if (!walletModel->isUnlockedForMintingOnly())
            return;

        walletModel->setWalletLocked(true);
        walletModel->setUnlockedForMintingOnly(false);
    }
}

void BitcoinGUI::backupWallet()
{
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Portfolio"), saveDir, tr("Portfolio Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the portfolio data to the new location."));
        }
    }
}

void BitcoinGUI::walletImport()
{
    QString workDir = QString::fromStdString(GetNuBitsDataDir().string());

    // Make getOpenFileName for one file, unlimited filters
    QFileDialog fd(this);
    fd.setDirectory(workDir);
    fd.setNameFilter(tr("Wallet Data (*.dat)"));
    fd.setFilter(QDir::AllDirs | QDir::AllEntries | QDir::Hidden | QDir::System); // show all that shit
    fd.setFileMode(QFileDialog::ExistingFile); // return one file name at [0]
    fd.setViewMode(QFileDialog::Detail); // show standard file detail view
    fd.exec();

    QString filename = fd.selectedFiles().value(0); // incase empty or cancel, use value() instead of at() for safe pointer
    QString passwd;
    QString rpcCmd;

    /** Cancel pressed */
    if(filename.trimmed().isEmpty())
        return;

    /** Attempt to begin the import, and detect fails */
    CWallet *openWallet = new CWallet(filename.toStdString());
    int importRet = openWallet->LoadWalletImport();

    if (importRet != DB_LOAD_OK)
    {
        if (importRet == DB_INCORRECT_UNIT)
            QMessageBox::warning(this, tr("Import Failed"), tr("Wallet import failed. Only NuShares wallets are supported."));
        else
            QMessageBox::warning(this, tr("Import Failed"), tr("Wallet import failed."));
        return;
    }

    /** Prompt for password, if necessary */
    if (openWallet->IsCrypted() && openWallet->IsLocked())
    {
         passwd = QInputDialog::getText(this, tr("Import Wallet"), tr("Password:"), QLineEdit::Password);
    }

    rpcCmd = QString("importnusharewallet \"%1\" \"%2\"")
            .arg(filename)
            .arg(passwd);
    passwd.clear();

    /** Threadlock friendly handoff to RPC */
    rpcConsole->cmdRequestFar(rpcCmd);
    rpcCmd.clear();

    QMessageBox::information(this, tr("Wallet imported"), tr("NuShares wallet import has been started. Check the debug console for details."));
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::exportDividendKeys()
{
    QMessageBox::StandardButton reply;

    QString sQuestion = tr("All your BlockShares private keys will be converted to Bitcoin private keys and imported into your Bitcoin wallet.\n\nThe Bitcoin wallet must be running, unlocked (if it was encrypted) and accept RPC commands.\n\nThis process may take several minutes because Bitcoin will scan the blockchain for transactions on all the imported keys.\n\nDo you want to proceed?");
    reply = QMessageBox::warning(this, tr("Bitcoin keys export confirmation"), sQuestion, QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    bool fMustLock = false;
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        fMustLock = true;
    }

    try {
        int iExportedCount, iErrorCount;
        walletModel->ExportDividendKeys(iExportedCount, iErrorCount);
        if (fMustLock)
            walletModel->setWalletLocked(true);
        QMessageBox::information(this,
                tr("Dividend keys export"),
                tr("%1 key(s) were exported to Bitcoin.\n%2 key(s) failed.")
                  .arg(iExportedCount)
                  .arg(iErrorCount)
                );
    }
    catch (std::runtime_error &e) {
        if (fMustLock)
            walletModel->setWalletLocked(true);
        QMessageBox::critical(this,
                tr("Dividend keys export"),
                tr("Error: %1").arg(e.what()));
    }
}

void BitcoinGUI::displayDividendKeys()
{
    bool fMustLock = false;
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        fMustLock = true;
    }

    try {
        std::vector<CDividendSecret> vSecret;
        walletModel->getDividendKeys(vSecret);
        if (fMustLock)
            walletModel->setWalletLocked(true);

        DividendKeysDialog dialog(this);
        dialog.setWindowTitle(this->windowTitle());
        dialog.setKeys(vSecret);
        dialog.exec();
    }
    catch (std::runtime_error &e) {
        if (fMustLock)
            walletModel->setWalletLocked(true);
        QMessageBox::critical(this,
                tr("Dividend keys display"),
                tr("Error: %1").arg(e.what()));
    }
}

void BitcoinGUI::showNormalIfMinimized()
{
    if(!isVisible()) // Show, if hidden
        show();
    if(isMinimized()) // Unminimize, if minimized
        showNormal();
}

void BitcoinGUI::distributeDividendsClicked()
{
    DistributeDivDialog dd(this);
    dd.exec();
}

void BitcoinGUI::switchUnitButtonClicked()
{
    gotoOverviewPage();

    changeUnit(switchUnitTarget);
}
