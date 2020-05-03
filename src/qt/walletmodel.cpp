#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"
#include "parktablemodel.h"
#include "bitcoinunits.h"

#include "ui_interface.h"
#include "wallet.h"
#include "walletdb.h" // for BackupWallet
#include "base58.h"
#include "datafeed.h"

#include <QSet>
#include <QTimer>

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    transactionTableModel(0),
    parkTableModel(0),
    cachedBalance(0), cachedUnconfirmedBalance(0), cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    pendingUpdate(false)
{
    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);
    parkTableModel = new ParkTableModel(wallet, this);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(processPendingUpdate()));
    timer->start(MODEL_UPDATE_DELAY);
}

WalletModel::~WalletModel()
{
    delete addressTableModel;
    addressTableModel = NULL;
    delete transactionTableModel;
    transactionTableModel = NULL;
    delete parkTableModel;
    parkTableModel = NULL;
}

qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}

qint64 WalletModel::getStake() const
{
    return wallet->GetStake();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

qint64 WalletModel::getParked() const
{
    return wallet->GetParked();
}

qint64 WalletModel::getMinTxFee() const
{
    LOCK(cs_main);
    return wallet->GetMinTxFee(pindexBest);
}

qint64 WalletModel::getMinTxOutAmount() const
{
    return wallet->GetMinTxOutAmount();
}

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

unsigned char WalletModel::getUnit() const
{
    return wallet->Unit();
}

void WalletModel::update()
{
    pendingUpdate = true;
    processPendingUpdate();
}

void WalletModel::processPendingUpdate()
{
    if (!pendingUpdate)
        return;

    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    qint64 newBalance = getBalance();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newParked = getParked();
    int newNumTransactions = getNumTransactions();
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedBalance != newBalance || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedParked != newParked)
        emit balanceChanged(newBalance, getStake(), newUnconfirmedBalance, newParked);

    if(cachedNumTransactions != newNumTransactions)
        emit numTransactionsChanged(newNumTransactions);

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);

    cachedBalance = newBalance;
    cachedUnconfirmedBalance = newUnconfirmedBalance;
    cachedParked = newParked;
    cachedNumTransactions = newNumTransactions;

    pendingUpdate = false;
}

void WalletModel::updateAddressList()
{
    addressTableModel->update();
}

bool WalletModel::validateAddress(const QString &address)
{
    CBitcoinAddress addressParsed(address.toStdString());
    return addressParsed.IsValid(getUnit());
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients, const CCoinControl *coinControl)
{
    qint64 total = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if(!validateAddress(rcp.address))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if(rcp.amount < getMinTxOutAmount())
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    // we do not use getBalance() here, because some coins could be locked or coin control could be active
    int64 nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl);
    BOOST_FOREACH(const COutput& out, vCoins)
        nBalance += out.tx->vout[out.i].nValue;

    if(total > nBalance) 
    {
        return AmountExceedsBalance;
    }

    if((total + getMinTxFee()) > nBalance)
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, getMinTxFee());
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        // Sendmany
        std::vector<std::pair<CScript, int64> > vecSend;
        foreach(const SendCoinsRecipient &rcp, recipients)
        {
            CScript scriptPubKey;
            scriptPubKey.SetDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
            vecSend.push_back(make_pair(scriptPubKey, rcp.amount));
        }

        CWalletTx wtx;
        CReserveKey keyChange(wallet);
        int64 nFeeRequired = 0;
        bool fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coinControl);

        if(!fCreated)
        {
            if((total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            return TransactionCreationFailed;
        }
        if(!ThreadSafeAskFee(nFeeRequired, tr("Sending...").toStdString(), wtx.cUnit))
        {
            return Aborted;
        }
        if(!wallet->CommitTransaction(wtx, keyChange))
        {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to to the address book
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        std::string strAddress = rcp.address.toStdString();
        CTxDestination dest = CBitcoinAddress(strAddress).Get();
        std::string strLabel = rcp.label.toStdString();
        {
            LOCK(wallet->cs_wallet);

            std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(dest);

            // Check if we have a new address or an updated label
            if (mi == wallet->mapAddressBook.end() || mi->second != strLabel)
            {
                wallet->SetAddressBookName(dest, strLabel);
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

QString WalletModel::park(qint64 amount, qint64 blocks, QString unparkAddress)
{
    {
        LOCK2(cs_main, wallet->cs_wallet);
        CWalletTx wtx;
        std::string result = wallet->Park(amount, blocks, CBitcoinAddress(unparkAddress.toStdString()), wtx, false);
        return QString::fromStdString(result);
    }
}

qint64 WalletModel::getNextPremium(qint64 amount, qint64 blocks)
{
    {
        LOCK2(cs_main, wallet->cs_wallet);
        if (!pindexBest)
            return 0;

        return pindexBest->GetNextPremium(amount, blocks, wallet->Unit());
    }
}

CVote WalletModel::getVote()
{
    {
        LOCK(wallet->cs_wallet);

        return wallet->vote;
    }
}

void WalletModel::setVote(const CVote& vote)
{
    {
        LOCK(wallet->cs_wallet);

        wallet->SetVote(vote);
    }
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

ParkTableModel *WalletModel::getParkTableModel()
{
    return parkTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if ((!was_locked) && isUnlockedForMintingOnly())
    {
        setWalletLocked(true);
        was_locked = getEncryptionStatus() == Locked;
    }
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked && !isUnlockedForMintingOnly());
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

void WalletModel::ExportDividendKeys(int &nExportedCount, int &nErrorCount)
{
    LOCK(wallet->cs_wallet);
    wallet->ExportDividendKeys(nExportedCount, nErrorCount);
}

void WalletModel::getDividendKeys(std::vector<CDividendSecret>& vSecret)
{
    LOCK(wallet->cs_wallet);
    wallet->DumpDividendKeys(vSecret);
}

bool WalletModel::getKey(const CKeyID &address, CKey& vchKeyOut) const
{
    return wallet->GetKey(address, vchKeyOut);
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    BOOST_FOREACH(const COutPoint& outpoint, vOutpoints)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, wallet->mapWallet[outpoint.hash].GetDepthInMainChain());
        vOutputs.push_back(out);
    }
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address) 
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);
    
    std::vector<COutPoint> vLockedCoins;
    
    // add locked coins
    BOOST_FOREACH(const COutPoint& outpoint, vLockedCoins)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, wallet->mapWallet[outpoint.hash].GetDepthInMainChain());
        vCoins.push_back(out);
    }
       
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        COutput cout = out;
        
        while (wallet->IsChange(cout.tx->vout[cout.i], *cout.tx) && cout.tx->vin.size() > 0 && wallet->IsMine(cout.tx->vin[0]))
        {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
        }

        CTxDestination address;
        if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) continue;
        mapCoins[CBitcoinAddress(address, cout.tx->cUnit).ToString().c_str()].push_back(out);
    }
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    return false;
}

void WalletModel::lockCoin(COutPoint& output)
{
    return;
}

void WalletModel::unlockCoin(COutPoint& output)
{
    return;
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    return;
}

bool WalletModel::isUnlockedForMintingOnly() const
{
    return wallet->fWalletUnlockMintOnly;
}

void WalletModel::setUnlockedForMintingOnly(bool fUnlockedForMintingOnly)
{
    wallet->fWalletUnlockMintOnly = fUnlockedForMintingOnly;
}

CDefaultKey WalletModel::getDefaultKey()
{
    return CDefaultKey(wallet);
}

CDataFeed WalletModel::getDataFeed() const
{
    return wallet->GetDataFeed();
}

void WalletModel::setDataFeed(const CDataFeed& dataFeed)
{
    wallet->SetDataFeed(dataFeed);
}

void WalletModel::updateFromDataFeed()
{
    try
    {
        UpdateFromDataFeed();
    }
    catch (std::exception& e)
    {
        throw WalletModelException(e.what());
    }
}

QString WalletModel::getDataFeedError() const
{
    return QString::fromStdString(strDataFeedError);
}

int WalletModel::getProtocolVersion() const
{
    LOCK(cs_main);
    if (!pindexBest)
        return 0;

    return pindexBest->nProtocolVersion;
}
