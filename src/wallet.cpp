// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"
#include "walletdb.h"
#include "crypter.h"
#include "ui_interface.h"
#include "base58.h"
#include "coincontrol.h" 
#include "kernel.h"
#include "bitcoinrpc.h"
#include "script.h"
#include "vote.h"
#include "datafeed.h"
#include <boost/algorithm/string/replace.hpp>

using namespace std;


//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

CPubKey CWallet::GenerateNewKey()
{
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    if (!AddKey(key))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CWallet::AddKey(const CKey& key)
{
    if (!CCryptoKeyStore::AddKey(key))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted())
        return CWalletDB(strWalletFile).WriteKey(key.GetPubKey(), key.GetPrivKey());
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey, const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret);
    }
    return false;
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    if (!IsLocked())
        return false;

    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64 nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                printf("Portfolio passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

void CWallet::SetVote(const CUserVote& vote)
{
    if (!vote.IsValid(pindexBest->nProtocolVersion))
        throw runtime_error("Cannot set invalid vote");

    if (this->vote != vote)
    {
        this->vote = vote;
        SaveVote();

        std::string strCmd = GetArg("-votenotify", "");
        printf("votenotify: %s\n", strCmd.c_str());
        if (!strCmd.empty())
            boost::thread t(runCommand, strCmd); // thread runs free

    }
}

void CWallet::SaveVote() const
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteVote(vote);
}

void CWallet::SaveDataFeed() const
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteDataFeed(dataFeed);
}

// This class implements an addrIncoming entry that causes pre-0.4
// clients to crash on startup if reading a private-key-encrypted wallet.
class CCorruptAddress
{
public:
    IMPLEMENT_SERIALIZE
    (
        if (nType & SER_DISK)
            READWRITE(nVersion);
    )
};

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion >= 40000)
        {
            // Versions prior to 0.4.0 did not support the "minversion" record.
            // Use a CCorruptAddress to make them crash instead.
            CCorruptAddress corruptAddress;
            pwalletdb->WriteSetting("addrIncoming", corruptAddress);
        }
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64 nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    printf("Encrypting portfolio with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); //We now probably have half of our keys encrypted in memory, and half not...die and let the user reload their unencrypted wallet.
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); //We now have keys encrypted in memory, but no on disk...die to avoid confusion and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);
    }

    return true;
}

void CWallet::WalletUpdateSpent(const CTransaction &tx)
{
    if (tx.cUnit != cUnit)
        return;
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                CWalletTx& wtx = (*mi).second;
                if (!wtx.IsSpent(txin.prevout.n) && IsMine(wtx.vout[txin.prevout.n]))
                {
                    printf("WalletUpdateSpent found spent unit %sshares %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                    wtx.MarkSpent(txin.prevout.n);
                    wtx.WriteToDisk();
                    vWalletUpdated.push_back(txin.prevout.hash);
                }
                if (setParked.count(txin.prevout))
                    RemoveParked(txin.prevout);
            }
        }
    }
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
            wtx.nTimeReceived = GetAdjustedTime();

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            fUpdated |= wtx.UpdateSpent(wtxIn.vfSpent);
        }

        //// debug print
        printf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString().substr(0,10).c_str(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;
#ifndef QT_GUI
        // If default receiving address gets used, replace it with a new one
        CScript scriptDefaultKey;
        scriptDefaultKey.SetDestination(vchDefaultKey.GetID());
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if (txout.scriptPubKey == scriptDefaultKey)
            {
                CPubKey newDefaultKey;
                if (GetKeyFromPool(newDefaultKey, false))
                {
                    SetDefaultKey(newDefaultKey);
                    SetAddressBookName(vchDefaultKey.GetID(), "");
                }
            }
        }
#endif
        // Notify UI
        UpdatedTransaction(hash);

        // nubit: Add parked outputs
        for (unsigned int i = 0; i < wtx.vout.size(); i++)
        {
            const CTxOut& txo = wtx.vout[i];

            int64 nDuration;
            CTxDestination unparkAddress;

            if (!ExtractPark(txo.scriptPubKey, nDuration, unparkAddress))
                continue;

            if (!HaveKey(boost::get<CKeyID>(unparkAddress)))
                continue;

            AddParked(COutPoint(wtx.GetHash(), i));
        }

        // since AddToWallet is called directly for self-originating transactions, check for consumption of own coins
        WalletUpdateSpent(wtx);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if (!strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }

    // Refresh UI
    MainFrameRepaint();
    return true;
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fFindBlock)
{
    if (tx.cUnit != cUnit)
        return false;
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);
        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate) return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);
            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(pblock);
            return AddToWallet(wtx);
        }
        else
            WalletUpdateSpent(tx);
    }
    return false;
}

bool CWallet::EraseFromWallet(uint256 hash)
{
    if (!fFileBacked)
        return false;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return true;
}


bool CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return true;
        }
    }
    return false;
}

int64 CWallet::GetDebit(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

bool CWallet::IsChange(const CTxOut& txout, const CTransaction& tx) const
{
    if (tx.cUnit != cUnit)
        return false;

    CTxDestination address;
    txnouttype type;

    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address, type) && ::IsMine(*this, address))
    {
        // Park transaction may be mine because the wallet has the unpark address, but it's never change
        if (type == TX_PARK)
            return false;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;

        // nubit: if the output address is the same as any input address, it is change (happens when avatar mode is enabled)
        if (GetBoolArg("-avatar", (cUnit == '8')))
        {
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
                if (mi != mapWallet.end())
                {
                    const CWalletTx& prev = (*mi).second;
                    if (txin.prevout.n < prev.vout.size())
                    {
                        const CTxOut& prevout = prev.vout[txin.prevout.n];
                        CTxDestination inAddress;
                        if (ExtractDestination(prevout.scriptPubKey, inAddress))
                        {
                            if (inAddress == address)
                                return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

int64 CWalletTx::GetTxTime() const
{
    return nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake() || IsCustodianGrant())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(int64& nGeneratedImmature, int64& nGeneratedMature, list<pair<CTxDestination, int64> >& listReceived,
                           list<pair<CTxDestination, int64> >& listSent, int64& nFee, string& strSentAccount) const
{
    nGeneratedImmature = nGeneratedMature = nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;
    const bool fCombine = (cUnit == '8');
    map<CTxDestination, int64> mapReceived;
    map<CTxDestination, int64> mapSent;

    if (IsCoinBase() || IsCoinStake())
    {
        if (GetBlocksToMaturity() > 0)
            nGeneratedImmature = pwallet->GetCredit(*this) - pwallet->GetDebit(*this);
        else
            nGeneratedMature = GetCredit() - GetDebit();
        return;
    }

    // Compute fee:
    int64 nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64 nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        const CTxOut& txout = vout[i];

        CTxDestination address;
        vector<unsigned char> vchPubKey;
        txnouttype type;
        if (!ExtractDestination(txout.scriptPubKey, address, type))
        {
            printf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                   this->GetHash().ToString().c_str());
        }

        // Don't report 'change' txouts
        if (nDebit > 0 && pwallet->IsChange(txout, *this))
            continue;

        if (nDebit > 0)
        {
            if (fCombine)
                mapSent[address] += txout.nValue;
            else
                listSent.push_back(make_pair(address, txout.nValue));
        }

        // Do not count parked amount as received
        if (type == TX_PARK)
            continue;

        if (pwallet->IsMine(txout))
        {
            if (fCombine)
                mapReceived[address] += txout.nValue;
            else
                listReceived.push_back(make_pair(address, txout.nValue));
        }
    }

    if (fCombine)
    {
        BOOST_FOREACH(const PAIRTYPE(CTxDestination, int64)& pair, mapReceived)
            listReceived.push_back(make_pair(pair.first, pair.second));
        BOOST_FOREACH(const PAIRTYPE(CTxDestination, int64)& pair, mapSent)
            listSent.push_back(make_pair(pair.first, pair.second));
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64& nGenerated, int64& nReceived, 
                                  int64& nSent, int64& nFee) const
{
    nGenerated = nReceived = nSent = nFee = 0;

    int64 allGeneratedImmature, allGeneratedMature, allFee;
    allGeneratedImmature = allGeneratedMature = allFee = 0;
    string strSentAccount;
    list<pair<CTxDestination, int64> > listReceived;
    list<pair<CTxDestination, int64> > listSent;
    GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);

    if (strAccount == "")
        nGenerated = allGeneratedMature;
    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64)& s, listSent)
            nSent += s.second;
        nFee = allFee;
    }
    {
        LOCK(pwallet->cs_wallet);
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64)& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.first))
            {
                map<CTxDestination, string>::const_iterator mi = pwallet->mapAddressBook.find(r.first);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.second;
            }
            else if (strAccount.empty())
            {
                nReceived += r.second;
            }
        }
    }
}

void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        BOOST_FOREACH(const CTxIn& txin, vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        {
            LOCK(pwallet->cs_wallet);
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
                if (mi != pwallet->mapWallet.end())
                {
                    tx = (*mi).second;
                    BOOST_FOREACH(const CMerkleTx& txWalletPrev, (*mi).second.vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (!fClient && txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                {
                    BOOST_FOREACH(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
                }
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWalletTx::WriteToDisk()
{
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;
    {
        LOCK(cs_wallet);
        while (pindex)
        {
            CBlock block;
            block.ReadFromDisk(pindex, true);
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = pindex->pnext;
        }
    }
    return ret;
}

int CWallet::ScanForWalletTransaction(const uint256& hashTx)
{
    CTransaction tx;
    tx.ReadFromDisk(COutPoint(hashTx, 0));
    if (AddToWalletIfInvolvingMe(tx, NULL, true, true))
        return 1;
    return 0;
}

void CWallet::ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    bool fRepeat = true;
    while (fRepeat)
    {
        LOCK(cs_wallet);
        fRepeat = false;
        vector<CDiskTxPos> vMissingTx;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            if ((wtx.IsCoinBase() && wtx.IsSpent(0)) || (wtx.IsCoinStake() && wtx.IsSpent(1)))
                continue;

            CTxIndex txindex;
            bool fUpdated = false;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (txindex.vSpent.size() != wtx.vout.size())
                {
                    printf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %d != wtx.vout.size() %d\n", txindex.vSpent.size(), wtx.vout.size());
                    continue;
                }
                for (unsigned int i = 0; i < txindex.vSpent.size(); i++)
                {
                    if (wtx.IsSpent(i))
                        continue;
                    if (!txindex.vSpent[i].IsNull() && IsMine(wtx.vout[i]))
                    {
                        wtx.MarkSpent(i);
                        fUpdated = true;
                        vMissingTx.push_back(txindex.vSpent[i]);
                    }
                }
                if (fUpdated)
                {
                    printf("ReacceptWalletTransactions found spent coin %sppc %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                    wtx.MarkDirty();
                    wtx.WriteToDisk();
                }
            }
            else
            {
                // Reaccept any txes of ours that aren't already in a block
                if (!(wtx.IsCoinBase() || wtx.IsCoinStake()))
                    wtx.AcceptWalletTransaction(txdb, false);
            }
        }
        if (!vMissingTx.empty())
        {
            // TODO: optimize this to scan just part of the block chain?
            if (ScanForWalletTransactions(pindexGenesisBlock))
                fRepeat = true;  // Found missing transactions: re-do Reaccept.
        }
    }
}

void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
    {
        if (!(tx.IsCoinBase() || tx.IsCoinStake() || tx.IsCustodianGrant()))
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayMessage(CInv(MSG_TX, hash), (CTransaction)tx);
        }
    }
    if (!(IsCoinBase() || IsCoinStake() || IsCustodianGrant()))
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            printf("Relaying wtx %s\n", hash.ToString().substr(0,10).c_str());
            RelayMessage(CInv(MSG_TX, hash), (CTransaction)*this);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}

void CWallet::ResendWalletTransactions()
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextTimeResendWalletTransactions)
        return;
    bool fFirst = (nNextTimeResendWalletTransactions == 0);
    nNextTimeResendWalletTransactions = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nTimeBestReceived < nLastTimeResendWalletTransactions)
        return;
    nLastTimeResendWalletTransactions = GetTime();

    // Rebroadcast any of our txes that aren't in a block yet
    printf("ResendWalletTransactions()\n");
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (nTimeBestReceived - (int64)wtx.nTimeReceived > 5 * 60)
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction())
                wtx.RelayWalletTransaction(txdb);
            else
                printf("ResendWalletTransactions() : CheckTransaction failed for transaction %s\n", wtx.GetHash().ToString().c_str());
        }
    }
}


void CWallet::CheckUnparkableOutputs()
{
    if (cUnit == '8')
        return;

    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions, but more frequently than ResendWalletTransactions
    if (GetTime() < nNextTimeCheckUnparkableOutputs)
        return;
    bool fFirst = (nNextTimeCheckUnparkableOutputs == 0);
    nNextTimeCheckUnparkableOutputs = GetTime() + GetRand(STAKE_TARGET_SPACING / 2);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nTimeBestReceived < nLastTimeCheckUnparkableOutputs)
        return;
    nLastTimeCheckUnparkableOutputs = GetTime();

    {
        LOCK(cs_wallet);
        if (!SendUnparkTransactions())
            printf("SendUnparkTransactions failed\n");
    }
}




//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64 CWallet::GetBalance() const
{
    int64 nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                continue;
            nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

int64 CWallet::GetUnconfirmedBalance() const
{
    int64 nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl) const
{
    vCoins.clear();

    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (!pcoin->IsFinal())
                continue;

            if (fOnlyConfirmed && !pcoin->IsConfirmed())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (!(pcoin->IsSpent(i)) && IsMine(pcoin->vout[i]) && pcoin->vout[i].nValue > 0 && !pcoin->vout[i].IsPark() &&
                (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                    vCoins.push_back(COutput(pcoin, i, pcoin->GetDepthInMainChain()));
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
int64 CWallet::GetStake() const
{
    int64 nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

int64 CWallet::GetNewMint() const
{
    int64 nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

int64 CWallet::GetParked() const
{
    int64 nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (!pcoin->IsSpent(i) && pcoin->IsParked(i) && IsMine(pcoin->vout[i]))
                nTotal += pcoin->vout[i].nValue;
    }
    return nTotal;
}


bool CWallet::SelectCoinsMinConf(int64 nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<int64, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<int64, pair<const CWalletTx*,unsigned int> > > vValue;
    int64 nTotalLower = 0;

    {
       LOCK(cs_wallet);
       vector<const CWalletTx*> vCoins;
       vCoins.reserve(mapWallet.size());
       for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
           vCoins.push_back(&(*it).second);
       random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

       BOOST_FOREACH(const CWalletTx* pcoin, vCoins)
       {
            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                if (pcoin->IsSpent(i) || !IsMine(pcoin->vout[i]) || pcoin->IsParked(i))
                    continue;

                if (pcoin->nTime > nSpendTime)
                    continue;  // ppcoin: timestamp must not exceed spend time

                int64 n = pcoin->vout[i].nValue;

                if (n <= 0)
                    continue;

                pair<int64,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin,i));

                if (n == nTargetValue)
                {
                    setCoinsRet.insert(coin.second);
                    nValueRet += coin.first;
                    return true;
                }
                else if (n < nTargetValue + CENT)
                {
                    vValue.push_back(coin);
                    nTotalLower += n;
                }
                else if (n < coinLowestLarger.first)
                {
                    coinLowestLarger = coin;
                }
            }
        }
    }

    if (nTotalLower == nTargetValue || nTotalLower == nTargetValue + CENT)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue + (coinLowestLarger.second.first ? CENT : 0))
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    if (nTotalLower >= nTargetValue + CENT)
        nTargetValue += CENT;

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend());
    vector<char> vfIncluded;
    vector<char> vfBest(vValue.size(), true);
    int64 nBest = nTotalLower;

    for (int nRep = 0; nRep < 1000 && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64 nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                if (nPass == 0 ? rand() % 2 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }

    // If the next larger is still closer, return it
    if (coinLowestLarger.second.first && coinLowestLarger.first - nTargetValue <= nBest - nTargetValue)
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        //// debug print
        if (fDebug && GetBoolArg("-printselectcoin"))
        {
            printf("SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
                if (vfBest[i])
                    printf("%s ", FormatMoney(vValue[i].first).c_str());
            printf("total %s\n", FormatMoney(nBest).c_str());
        }
    }

    return true;
}

bool CWallet::SelectCoins(int64 nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet, const CCoinControl* coinControl) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    return (SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 6, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 1, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 0, 1, setCoinsRet, nValueRet));
}




bool CWallet::CreateTransaction(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, const CCoinControl* coinControl)
{
    int64 nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = 1;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;
                wtxNew.cUnit = cUnit;

                int64 nTotalValue = nValue + nFeeRet;
                double dPriority = 0;
                // vouts to the payees
                BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
                {
                    // nu: split shares if appropriate
                    wtxNew.AddOutput(s.first, s.second);
                }

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                int64 nValueIn = 0;
                if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins, nValueIn, coinControl))
                    return false;
                CScript scriptChange;
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
                {
                    int64 nCredit = pcoin.first->vout[pcoin.second].nValue;
                    dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain();
                    scriptChange = pcoin.first->vout[pcoin.second].scriptPubKey;
                }

                int64 nChange = nValueIn - nValue - nFeeRet;
                // if sub-cent change is required, the fee must be raised to at least MIN_TX_FEE
                // or until nChange becomes zero
                // NOTE: this depends on the exact behaviour of GetMinFee
                if (nFeeRet < wtxNew.GetSafeUnitMinFee(pindexBest) && nChange > 0 && nChange < CENT)
                {
                    int64 nMoveToFee = min(nChange, wtxNew.GetSafeUnitMinFee(pindexBest) - nFeeRet);
                    nChange -= nMoveToFee;
                    nFeeRet += nMoveToFee;
                }

                // ppcoin: sub-cent change is moved to fee
                if (nChange > 0 && nChange < wtxNew.GetMinTxOutAmount())
                {
                    nFeeRet += nChange;
                    nChange = 0;
                }

                if (nChange > 0)
                    wtxNew.AddChange(nChange, scriptChange, coinControl, reservekey);
                else
                    reservekey.ReturnKey();

                // Fill vin
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    if (!SignSignature(*this, *coin.first, wtxNew, nIn++))
                        return false;

                // Limit size
                unsigned int nBytes = wtxNew.GetSize();
                if (nBytes >= MAX_BLOCK_SIZE/4)
                    return false;
                dPriority /= nBytes;

                // Check that enough fee is included
                int64 nMinFee = wtxNew.GetSafeMinFee(pindexBest, nBytes);

                if (nFeeRet < nMinFee)
                {
                    nFeeRet = nMinFee;
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, coinControl);
}

bool CWallet::CreateBurnTransaction(int64 nBurnValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, const CCoinControl* coinControl)
{
    if (nBurnValue < 0)
        return false;

    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = GetSafeMinTxFee(pindexBest);
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;
                wtxNew.cUnit = cUnit;

                // Guarantee that there will be always a change output
                int64 nMinChange = wtxNew.GetMinTxOutAmount();
                int64 nBurnOrFee = max(nBurnValue, nFeeRet);
                int64 nTotalValue = nBurnOrFee + nMinChange;

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                int64 nValueIn = 0;
                if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins, nValueIn, coinControl))
                    return false;

                CScript scriptChange;
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
                {
                    scriptChange = pcoin.first->vout[pcoin.second].scriptPubKey;
                }

                // Set the change output
                int64 nChange = nValueIn - nBurnOrFee;
                assert(nChange >= nMinChange);
                wtxNew.AddChange(nChange, scriptChange, coinControl, reservekey);

                // Fill vin
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    if (!SignSignature(*this, *coin.first, wtxNew, nIn++))
                        return false;

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_BLOCK_SIZE/4)
                    return false;

                // Check that enough fee is included
                int64 nPayFee = wtxNew.GetSafeUnitMinFee(pindexBest) * (1 + (int64)nBytes / 1000);
                int64 nMinFee = wtxNew.GetSafeMinFee(pindexBest, nBytes);

                if (nBurnOrFee < max(nPayFee, nMinFee))
                {
                    nFeeRet = max(nPayFee, nMinFee);
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

static map<const CWalletTx*, uint256> mapTxHash;
static map<const CWalletTx*, CTxIndex> mapTxIndex;
static map<const CWalletTx*, CBlock> mapTxBlock;
static map<const CWalletTx*, int64> mapTxLastUse;

#ifdef TESTING
extern int nForcedVersionVote;
#endif

void RemoveVotedAssets(CBlockIndex& pindexdummy, vector<CAssetVote>& vAssetVotes)
{
    CAsset asset;
    set<CAssetVote> assetVotesToRemove;
    BOOST_FOREACH(const CAssetVote& assetVote, vAssetVotes)
    {
        asset.SetNull();
        if (pindexdummy.GetVotedAsset(assetVote.nAssetId, asset) && assetVote.ProducesAsset(asset))
            assetVotesToRemove.insert(assetVote);
    }

    BOOST_FOREACH(const CAssetVote& assetVote, assetVotesToRemove)
    {
        printf("Removing already voted asset %d\n", assetVote.nAssetId);
        vAssetVotes.erase(std::remove(vAssetVotes.begin(), vAssetVotes.end(), assetVote), vAssetVotes.end());
    }
}

// ppcoin: create coin stake transaction
bool CWallet::CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, int64 nSearchInterval, CTransaction& txNew, CBlockIndex* pindexprev)
{
    CBigNum bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    LOCK2(cs_main, cs_wallet);

    // remove from cache the unused transactions
    int64 nNow = GetTime();
    map<const CWalletTx*, int64>::iterator it = mapTxLastUse.begin();
    while (it != mapTxLastUse.end())
    {
        const CWalletTx* wtx = it->first;
        int64& nLastUse = it->second;

        if (nNow > nLastUse + 24 * 60 * 60)
        {
            mapTxHash.erase(wtx);
            mapTxIndex.erase(wtx);
            mapTxBlock.erase(wtx);
            mapTxLastUse.erase(it++);
        }
        else
            it++;
    }
    CleanStakeModifierCache();

    txNew.vin.clear();
    txNew.vout.clear();
    txNew.cUnit = cUnit;
    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    txNew.vout.push_back(CTxOut(0, scriptEmpty));
    // Choose coins to use
    int64 nBalance = GetBalance();
    int64 nReserveBalance = 0;
    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        return error("CreateCoinStake : invalid reserve balance amount");
    if (nBalance <= nReserveBalance)
        return false;
    set<pair<const CWalletTx*,unsigned int> > setCoins;
    vector<const CWalletTx*> vwtxPrev;
    int64 nValueIn = 0;
    if (!SelectCoins(nBalance - nReserveBalance, txNew.nTime, setCoins, nValueIn))
    {
        if (GetBoolArg("-debugmint"))
            printf("Minting: unable to select coins\n");
        return false;
    }
    if (setCoins.empty())
    {
        if (GetBoolArg("-debugmint"))
            printf("Minting: unable to set coins\n");
        return false;
    }
    int64 nCredit = 0;
    CScript scriptPubKeyKernel;
    int nOutputs = -1;

    map<string, int> mapFailCount;
    bool fKernelFound = false;

    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        mapTxLastUse[pcoin.first] = nNow;

        if (pcoin.first->vout[pcoin.second].nValue < MIN_COINSTAKE_VALUE)
        {
            if (GetBoolArg("-debugmint"))
                mapFailCount["Value below minimum value"]++;
            continue; // nu: only count coins meeting min value requirement
        }

        map<const CWalletTx*, uint256>::const_iterator itHash = mapTxHash.find(pcoin.first);
        if (itHash == mapTxHash.end())
        {
            pair<const CWalletTx*, uint256> value(pcoin.first, pcoin.first->GetHash());
            itHash = mapTxHash.insert(value).first;
        }
        const uint256& txHash = itHash->second;

        map<const CWalletTx*, CTxIndex>::const_iterator itTxIndex = mapTxIndex.find(pcoin.first);
        if (itTxIndex == mapTxIndex.end())
        {
            pair<const CWalletTx*, CTxIndex> value(pcoin.first, CTxIndex());
            CTxDB txdb("r");
            if (!txdb.ReadTxIndex(txHash, value.second))
            {
                if (GetBoolArg("-debugmint"))
                    mapFailCount["Unable to load transaction from database"]++;
                continue;
            }
            itTxIndex = mapTxIndex.insert(value).first;
        }
        const CTxIndex& txindex = itTxIndex->second;

        // Read block header
        map<const CWalletTx*, CBlock>::const_iterator itTxBlock = mapTxBlock.find(pcoin.first);
        if (itTxBlock == mapTxBlock.end())
        {
            pair<const CWalletTx*, CBlock> value(pcoin.first, CBlock());
            if (!value.second.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
            {
                if (GetBoolArg("-debugmint"))
                    mapFailCount["Unable to read transaction block from database"]++;
                continue;
            }
            itTxBlock = mapTxBlock.insert(value).first;
        }
        const CBlock& block = itTxBlock->second;

        static int nMaxStakeSearchInterval = 60;
        if (block.GetBlockTime() + nStakeMinAge > txNew.nTime - nMaxStakeSearchInterval)
        {
            if (GetBoolArg("-debugmint"))
                mapFailCount["Block does not meet min age requirement"]++;
            continue; // only count coins meeting min age requirement
        }

        fKernelFound = false;
        for (unsigned int n=0; n<min(nSearchInterval,(int64)nMaxStakeSearchInterval) && !fKernelFound && !fShutdown; n++)
        {
            // Search backward in time from the given txNew timestamp 
            // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
            uint256 hashProofOfStake = 0;
            COutPoint prevoutStake = COutPoint(txHash, pcoin.second);
            string failReason;
            fKernelFound = CheckStakeKernelHash(nBits, block, txindex.pos.nTxPos - txindex.pos.nBlockPos, *pcoin.first, prevoutStake, txNew.nTime - n, hashProofOfStake, false, GetBoolArg("-debugmint") ? &failReason : NULL);
            if (!fKernelFound && GetBoolArg("-debugmint"))
                mapFailCount[failReason]++;
            if (fKernelFound)
            {
                // Found a kernel
                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : kernel found\n");
                vector<valtype> vSolutions;
                txnouttype whichType;
                CScript scriptPubKeyOut;
                scriptPubKeyKernel = pcoin.first->vout[pcoin.second].scriptPubKey;
                if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                        printf("CreateCoinStake : failed to parse kernel\n", whichType);
                    break;
                }
                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : parsed kernel type=%d\n", whichType);
                if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                        printf("CreateCoinStake : no support for kernel type=%d\n", whichType);
                    break;  // only support pay to public key and pay to address
                }
                if (whichType == TX_PUBKEYHASH) // pay to address type
                {
                    // convert to pay to public key type
                    CKey key;
                    if (!keystore.GetKey(uint160(vSolutions[0]), key))
                    {
                        if (fDebug && GetBoolArg("-printcoinstake"))
                            printf("CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        break;  // unable to find corresponding public key
                    }
                    scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
                }
                else
                    scriptPubKeyOut = scriptPubKeyKernel;

                txNew.nTime -= n; 
                txNew.vin.push_back(CTxIn(txHash, pcoin.second));
                nCredit += pcoin.first->vout[pcoin.second].nValue;
                vwtxPrev.push_back(pcoin.first);
                txNew.vout.push_back(CTxOut(0, scriptPubKeyOut));
                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : added kernel type=%d\n", whichType);
                break;
            }
        }
        if (fKernelFound || fShutdown)
            break; // if kernel is found stop searching
    }

    if (!fKernelFound && GetBoolArg("-debugmint"))
    {
        printf("Minting: unable to find kernel. Reasons:\n");
        BOOST_FOREACH(PAIRTYPE(const string, int) pair, mapFailCount)
        {
            const string& sReason = pair.first;
            const int& nCount = pair.second;
            printf("  %s: %d times\n", sReason.c_str(), nCount);
        }
    }

    if (nCredit == 0 || nCredit > nBalance - nReserveBalance)
        return false;
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        map<const CWalletTx*, uint256>::const_iterator itHash = mapTxHash.find(pcoin.first);
        if (itHash == mapTxHash.end())
        {
            pair<const CWalletTx*, uint256> value(pcoin.first, pcoin.first->GetHash());
            itHash = mapTxHash.insert(value).first;
        }
        const uint256& txHash = itHash->second;

        // Attempt to add more inputs
        // Only add coins of the same key/address as kernel
        if (txNew.vout.size() == 2 && ((pcoin.first->vout[pcoin.second].scriptPubKey == scriptPubKeyKernel || pcoin.first->vout[pcoin.second].scriptPubKey == txNew.vout[1].scriptPubKey))
            && txHash != txNew.vin[0].prevout.hash)
        {
            // Stop adding more inputs if already too many inputs
            if (txNew.vin.size() >= 5)
                break;
            // Stop adding inputs if reached reserve limit
            if (nCredit + pcoin.first->vout[pcoin.second].nValue > nBalance - nReserveBalance)
                break;
            // nu: Do not add inputs able to find a block
            if (pcoin.first->vout[pcoin.second].nValue >= MIN_COINSTAKE_VALUE)
                continue;
            // nu: Do not add unconfirmed transactions
            if (pcoin.first->GetDepthInMainChain() == 0)
                continue;
            txNew.vin.push_back(CTxIn(txHash, pcoin.second));
            nCredit += pcoin.first->vout[pcoin.second].nValue;
            vwtxPrev.push_back(pcoin.first);
        }
    }

    // nu: split outputs
    if (nSplitShareOutputs > 0)
    {
        nOutputs = nCredit / nSplitShareOutputs;

        // limit the number of outputs to avoid exceeding MAX_COINSTAKE_SIZE
        if (nOutputs > 5)
            nOutputs = 5;

        for (int i = 1; i < nOutputs; i++)
            txNew.vout.push_back(CTxOut(0, txNew.vout[1].scriptPubKey));
    }

    nCredit += GetProofOfStakeReward();

    int nProtocolVersion = GetProtocolForNextBlock(pindexprev);

    CVote blockVote = vote.GenerateBlockVote(nProtocolVersion);

    // nubit: Add current vote
    if (!blockVote.IsValidInBlock(nProtocolVersion))
        return error("CreateCoinStake : generated vote is invalid");

#ifdef TESTING
    if (nForcedVersionVote != -1)
        blockVote.nVersionVote = nForcedVersionVote;
#endif

    {
        CBlockIndex pindexdummy;
        pindexdummy.pprev = pindexprev;
        pindexdummy.nTime = txNew.nTime;

        CalculateVotedAssets(&pindexdummy);
        RemoveVotedAssets(pindexdummy, blockVote.vAssetVote);
    }

    txNew.vout.push_back(CTxOut(0, blockVote.ToScript(nProtocolVersion)));

    CBlockIndex pindexdummy;
    pindexdummy.pprev = pindexprev;
    pindexdummy.nTime = txNew.nTime;
    pindexdummy.vote = blockVote;
    pindexdummy.nProtocolVersion = nProtocolVersion;

    {
        CTxDestination addressSigner;
        int64 nReward;
        if (!CalculateSignerReward(&pindexdummy, addressSigner, nReward))
            return error("CreateCoinStake : unable to get signer reward");

        if (nReward > 0)
        {
            CScript scriptOutput;
            scriptOutput.SetDestination(addressSigner);
            txNew.vout.push_back(CTxOut(nReward, scriptOutput));
        }
    }

    // nubit: The result of the vote is stored in the CoinStake transaction
    CParkRateVote parkRateResult;

    {
        CTxDB txdb("r");
        if (!txNew.GetCoinAge(txdb, blockVote.nCoinAgeDestroyed))
            return error("CreateCoinStake : failed to calculate coin age");
    }

    vector<CParkRateVote> vParkRateResult;
    if (!CalculateParkRateResults(blockVote, pindexprev, nProtocolVersion, vParkRateResult))
        return error("CalculateParkRateResults failed");

    BOOST_FOREACH(const CParkRateVote& parkRateResult, vParkRateResult)
        txNew.vout.push_back(CTxOut(0, parkRateResult.ToParkRateResultScript()));

    CalculateVotedFees(&pindexdummy);

    int64 nMinFee = 0;
    loop
    {
        // Set output amount
        int64 nAmountToDistribute = nCredit - nMinFee;
        int i;
        for (i = 1; i < nOutputs; i++)
        {
            txNew.vout[i].nValue = nSplitShareOutputs;
            nAmountToDistribute -= txNew.vout[i].nValue;
        }
        txNew.vout[i].nValue = nAmountToDistribute;

        // Sign
        int nIn = 0;
        BOOST_FOREACH(const CWalletTx* pcoin, vwtxPrev)
        {
            if (!SignSignature(*this, *pcoin, txNew, nIn++))
                return error("CreateCoinStake : failed to sign coinstake");
        }

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBytes >= MAX_COINSTAKE_SIZE)
            return error("CreateCoinStake : exceeded coinstake size limit");

        // Check enough fee is paid
        if (nMinFee < txNew.GetMinFee(&pindexdummy) - txNew.GetUnitMinFee(&pindexdummy))
        {
            nMinFee = txNew.GetMinFee(&pindexdummy) - txNew.GetUnitMinFee(&pindexdummy);
            continue; // try signing again
        }
        else
        {
            if (fDebug && GetBoolArg("-printfee"))
                printf("CreateCoinStake : fee for coinstake %s\n", FormatMoney(nMinFee).c_str());
            break;
        }
    }

    // Successfully generated coinstake
    return true;
}

bool CWallet::CreateUnparkTransaction(CWalletTx& wtxParked, unsigned int nOut, const CBitcoinAddress& unparkAddress, int64 nAmount, CWalletTx& wtxNew)
{
    return CreateUnparkTransaction(wtxParked.GetHash(), nOut, unparkAddress, nAmount, wtxNew);
}

bool CWallet::CreateUnparkTransaction(const uint256& hashPark, unsigned int nOut, const CBitcoinAddress& unparkAddress, int64 nAmount, CWalletTx& wtxNew)
{
    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            wtxNew.vin.clear();
            wtxNew.vout.clear();
            wtxNew.fFromMe = true;
            wtxNew.cUnit = cUnit;

            CScript scriptPubKey;
            scriptPubKey.SetDestination(unparkAddress.Get());
            wtxNew.vout.push_back(CTxOut(nAmount, scriptPubKey));

            CScript scriptSig;
            scriptSig.SetUnpark();
            wtxNew.vin.push_back(CTxIn(hashPark, nOut, scriptSig));

            // Fill vtxPrev by copying from previous transactions vtxPrev
            wtxNew.AddSupportingTransactions(txdb);
            wtxNew.fTimeReceivedIsTxTime = true;
        }
    }
    return true;
}

bool CWallet::SendUnparkTransactions(vector<CWalletTx>& vtxRet)
{
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx& wtx = it->second;
        for (unsigned int i = 0; i < wtx.vout.size(); i++)
        {
            if (wtx.IsSpent(i))
                continue;

            const CTxOut& txo = wtx.vout[i];

            if (!IsMine(txo))
                continue;

            int64 nDuration;

            CTxDestination unparkDestination;
            if (!ExtractPark(txo.scriptPubKey, nDuration, unparkDestination))
                continue;
            CBitcoinAddress unparkAddress(unparkDestination, wtx.cUnit);

            CBlockIndex *pindex = NULL;
            int64 nDepth = wtx.GetDepthInMainChain(pindex);

            if (nDepth < nDuration)
                continue;

            if (!pindex)
                continue;

            int64 nPremium = pindex->GetPremium(txo.nValue, nDuration, wtx.cUnit);
            int64 nAmount = txo.nValue + nPremium;

            printf("Found unparkable output: hash=%s output=%d unit=%c value=%" PRI64u " duration=%" PRI64u " unparkAddress=%s premium=%" PRI64u "\n",
                    wtx.GetHash().GetHex().c_str(), i, wtx.cUnit, txo.nValue, nDuration, unparkAddress.ToString().c_str(), nPremium);

            CWalletTx wtxUnpark;
            if (!CreateUnparkTransaction(wtx, i, unparkAddress, nAmount, wtxUnpark))
                return false;

            CReserveKey reservekey(this);
            if (!CommitTransaction(wtxUnpark, reservekey))
                return false;

            vtxRet.push_back(wtxUnpark);
        }
    }

    return true;
}

// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    {
        LOCK2(cs_main, cs_wallet);
        printf("CommitTransaction:\n%s", wtxNew.ToString().c_str());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                coin.MarkSpent(txin.prevout.n);
                coin.WriteToDisk();
                UpdatedTransaction(coin.GetHash());
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptToMemoryPool())
        {
            // This must not fail. The transaction has already been signed and recorded.
            printf("CommitTransaction() : Error: Transaction not valid");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    MainFrameRepaint();
    return true;
}




string CWallet::SendMoney(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64 nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Portfolio locked, unable to create transaction  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Portfolio unlocked for block minting only, unable to create transaction.");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (!CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: Not enough funds. This transaction requires a transaction fee of at least %s  "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }

    if (fAskFee && !ThreadSafeAskFee(nFeeRequired, _("Sending..."), wtxNew.cUnit))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the shares in your portfolio were already spent, such as if you used a copy of wallet.dat and shares were spent in the copy but not marked as spent here.");

    MainFrameRepaint();
    return "";
}



string CWallet::SendMoneyToDestination(const CTxDestination& address, int64 nValue, CWalletTx& wtxNew, bool fAskFee)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");
    if (nValue + GetSafeMinTxFee(pindexBest) > GetBalance())
        return _("Insufficient funds");

    // Parse bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, wtxNew, fAskFee);
}


std::string CWallet::Park(int64 nValue, int64 nDuration, const CBitcoinAddress& unparkAddress, CWalletTx& wtxNew, bool fAskFee)
{
    if (cUnit == '8')
        return _("Cannot park shares");

    if (!ParkDurationRange(nDuration))
        return _("Invalid park duration");

    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");
    if (nValue + GetSafeMinTxFee(pindexBest) > GetBalance())
        return _("Insufficient funds");

    int64 nPremium = pindexBest->GetNextPremium(nValue, nDuration, cUnit);

    if (nPremium == 0)
        return _("No premium for this duration");

    if (!MoneyRange(nValue + nPremium))
        return _("Expected return is out of range");

    // Generate parking output script
    CScript script;
    CKeyID unparkID;
    if (!unparkAddress.GetKeyID(unparkID))
        return _("Invalid unpark address");

    script.SetPark(nDuration, unparkID);

    // Verify result
    {
        CTxDestination extractedDestination;
        int64 nExtractedDuration;
        if (!ExtractPark(script, nExtractedDuration, extractedDestination))
            return _("Verification of parking script failed");
        CBitcoinAddress extractedAddress(extractedDestination, cUnit);
        if (extractedAddress != unparkAddress)
            return _("Verification of parking script failed");
        if (nExtractedDuration != nDuration)
            return _("Verification of parking script failed");
    }

    return SendMoney(script, nValue, wtxNew, fAskFee);
}


string CWallet::BurnMoney(int64 nValue, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64 nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Portfolio locked, unable to create transaction  ");
        printf("BurnMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Portfolio unlocked for block minting only, unable to create transaction.");
        printf("BurnMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (!CreateBurnTransaction(nValue, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: Not enough funds. This transaction requires a transaction fee of at least %s  "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("BurnMoney() : %s\n", strError.c_str());
        return strError;
    }

    if (fAskFee && !ThreadSafeAskFee(nFeeRequired, _("Sending..."), wtxNew.cUnit))
        return "ABORTED";


    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the shares in your portfolio were already spent, such as if you used a copy of wallet.dat and shares were spent in the copy but not marked as spent here.");

    MainFrameRepaint();
    return "";
}


int CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return false;
    fFirstRunRet = false;
    int nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
        nLoadWalletRet = DB_NEED_REWRITE;
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    CreateThread(ThreadFlushWalletDB, &strWalletFile);
    return DB_LOAD_OK;
}

int CWallet::LoadWalletImport()
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    int nLoadWalletRet = CWalletDB(strWalletFile,"r+").LoadWalletImport(this);
    if (nLoadWalletRet != DB_LOAD_OK) {
        return nLoadWalletRet;
    }

    return DB_LOAD_OK;
}

bool CWallet::SetAddressBookName(const CTxDestination& address, const string& strName)
{
    mapAddressBook[address] = strName;
    AddressBookRepaint();
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address, cUnit).ToString(), strName);
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    mapAddressBook.erase(address);
    AddressBookRepaint();
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address, cUnit).ToString());
}


void CWallet::PrintWallet(const CBlock& block)
{
    {
        LOCK(cs_wallet);
        if (block.IsProofOfWork() && mapWallet.count(block.vtx[0].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
            printf("    mine:  %d  %d  %s", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), FormatMoney(wtx.GetCredit()).c_str());
        }
        if (block.IsProofOfStake() && mapWallet.count(block.vtx[1].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[1].GetHash()];
            printf("    stake: %d  %d  %s", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), FormatMoney(wtx.GetCredit()).c_str());
        }
    }
    printf("\n");
}

bool CWallet::GetTransaction(const uint256 &hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
        {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

bool GetWalletFile(CWallet* pwallet, string &strWalletFileOut)
{
    if (!pwallet->fFileBacked)
        return false;
    strWalletFileOut = pwallet->strWalletFile;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        BOOST_FOREACH(int64 nIndex, setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64 nKeys = max(GetArg("-keypool", 100), (int64)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64 nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        printf("CWallet::NewKeyPool wrote %"PRI64d" new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool()
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize = max(GetArg("-keypool", 100), 0LL);
        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64 nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            printf("keypool added key %"PRI64d", size=%d\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        printf("keypool reserve %"PRI64d"\n", nIndex);
    }
}

int64 CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64 nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64 nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    printf("keypool keep %"PRI64d"\n", nIndex);
}

void CWallet::ReturnKey(int64 nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    if (fDebug && GetBoolArg("-printkeypool"))
        printf("keypool return %"PRI64d"\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool fAllowReuse)
{
    int64 nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (fAllowReuse && vchDefaultKey.IsValid())
            {
                result = vchDefaultKey;
                return true;
            }
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64 CWallet::GetOldestKeyPoolTime()
{
    int64 nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

// ppcoin: check 'spent' consistency between wallet and txindex
// ppcoin: fix wallet spent state according to txindex
void CWallet::FixSpentCoins(int& nMismatchFound, int64& nBalanceInQuestion, bool fCheckOnly)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;

    LOCK(cs_wallet);
    vector<CWalletTx*> vCoins;
    vCoins.reserve(mapWallet.size());
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        vCoins.push_back(&(*it).second);

    CTxDB txdb("r");
    BOOST_FOREACH(CWalletTx* pcoin, vCoins)
    {
        // Find the corresponding transaction index
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin->GetHash(), txindex))
            continue;
        for (int n=0; n < pcoin->vout.size(); n++)
        {
            if (IsMine(pcoin->vout[n]) && pcoin->IsSpent(n) && (txindex.vSpent.size() <= n || txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found lost unit %sshares %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkUnspent(n);
                    pcoin->WriteToDisk();
                }
            }
            else if (IsMine(pcoin->vout[n]) && !pcoin->IsSpent(n) && (txindex.vSpent.size() > n && !txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found spent unit %sshares %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkSpent(n);
                    pcoin->WriteToDisk();
                }
            }
        }
    }
}

// ppcoin: disable transaction (only for coinstake)
void CWallet::DisableTransaction(const CTransaction &tx)
{
    if (!tx.IsCoinStake() || !IsFromMe(tx))
        return; // only disconnecting coinstake requires marking input unspent

    LOCK(cs_wallet);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size() && IsMine(prev.vout[txin.prevout.n]))
            {
                prev.MarkUnspent(txin.prevout.n);
                prev.WriteToDisk();
            }
        }
    }
}

CPubKey CReserveKey::GetReservedKey()
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else
        {
            printf("CReserveKey::GetReservedKey(): Warning: using default key instead of a new key, top up your keypool.");
            vchPubKey = pwallet->vchDefaultKey;
        }
    }
    assert(vchPubKey.IsValid());
    return vchPubKey;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress)
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(const int64& id, setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::ExportDividendKeys(int &nExportedCount, int &nErrorCount)
{
    if (cUnit != '8')
        throw runtime_error("Currency wallets will not receive dividends. Refusing to export keys to Bitcoin.");

    nExportedCount = 0;
    nErrorCount = 0;

    if (IsLocked())
        throw runtime_error("The portfolio is locked. Please unlock it first.");
    if (fWalletUnlockMintOnly)
        throw runtime_error("Portfolio is unlocked for minting only.");

    LOCK(cs_wallet);
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, string)& item, mapAddressBook)
    {
        const CBitcoinAddress address(item.first, cUnit);
        CSecret vchSecret;
        bool fCompressed;
        if (address.IsScript(cUnit))
        {
            CScriptID scriptID = boost::get<CScriptID>(address.Get());
            CScript script;
            if (!GetCScript(scriptID, script))
            {
                printf("Failed get script of address %s\n", address.ToString().c_str());
                nErrorCount++;
                continue;
            }

            txnouttype type;
            std::vector<CTxDestination> vDestination;
            int nRequired;
            if (!ExtractDestinations(script, type, vDestination, nRequired))
            {
                printf("Failed extract addresses from address %s\n", address.ToString().c_str());
                nErrorCount++;
                continue;
            }

            if (type != TX_MULTISIG)
            {
                printf("Address %s is not a multisig address\n", address.ToString().c_str());
                nErrorCount++;
                continue;
            }

            json_spirit::Array vDividendAddressStrings;
            BOOST_FOREACH(const CTxDestination &destination, vDestination)
                vDividendAddressStrings.push_back(CDividendAddress(destination).ToString());

            json_spirit::Array params;
            params.push_back(json_spirit::Value(nRequired));
            params.push_back(vDividendAddressStrings);
            params.push_back("BlockShares");

            try
            {
                string result = CallDividendRPC("addmultisigaddress", params);
                printf("Exported multisig address %s: %s\n", address.ToString().c_str(), result.c_str());
                nExportedCount++;
            }
            catch (dividend_rpc_error &error)
            {
                printf("Failed to add multisig address of address %s: %s\n", address.ToString().c_str(), error.what());
                nErrorCount++;
            }
        }
        else
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
            {
                printf("Unable to get key ID for address %s\n", address.ToString().c_str());
                nErrorCount++;
                continue;
            }
            if (!GetSecret(keyID, vchSecret, fCompressed))
            {
                printf("Private key for address %s is not known\n", address.ToString().c_str());
                nErrorCount++;
                continue;
            }

            json_spirit::Array params;
            params.push_back(CDividendSecret(vchSecret, fCompressed).ToString());
            params.push_back("BlockShares");
            try
            {
                string result = CallDividendRPC("importprivkey", params);
                printf("Exported private key of address %s: %s\n", address.ToString().c_str(), result.c_str());
                nExportedCount++;
            }
            catch (dividend_rpc_error &error)
            {
                printf("Failed to export private key of address %s: %s\n", address.ToString().c_str(), error.what());
                nErrorCount++;
            }
        }
    }
}

void CWallet::DumpDividendKeys(vector<CDividendSecret>& vSecret)
{
    if (cUnit != '8')
        throw runtime_error("Currency wallets will not receive dividends. Refusing to export keys to Bitcoin.");

    if (IsLocked())
        throw runtime_error("The portfolio is locked. Please unlock it first.");
    if (fWalletUnlockMintOnly)
        throw runtime_error("Portfolio is unlocked for minting only.");

    LOCK(cs_wallet);
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, string)& item, mapAddressBook)
    {
        const CBitcoinAddress address(item.first, cUnit);
        CSecret vchSecret;
        bool fCompressed;
        if (address.IsScript(cUnit))
            continue;
        else
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
            {
                printf("Unable to get key ID for address %s\n", address.ToString().c_str());
                continue;
            }
            if (!GetSecret(keyID, vchSecret, fCompressed))
            {
                printf("Private key for address %s is not known\n", address.ToString().c_str());
                continue;
            }

            vSecret.push_back(CDividendSecret(vchSecret, fCompressed));
        }
    }
}

void CWallet::AddParked(const COutPoint& outpoint)
{
    setParked.insert(outpoint);
    CWalletDB(strWalletFile).WriteParked(setParked);
}

void CWallet::RemoveParked(const COutPoint& outpoint)
{
    setParked.erase(outpoint);
    CWalletDB(strWalletFile).WriteParked(setParked);
}