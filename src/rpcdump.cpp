// Copyright (c) 2009-2012 Bitcoin Developers
// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2013-2014 The Peershares developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h" // for pwalletMain
#include "bitcoinrpc.h"
#include "ui_interface.h"
#include "base58.h"

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#define printf OutputDebugStringF

// using namespace boost::asio;
using namespace json_spirit;
using namespace std;

extern boost::thread_specific_ptr<CWallet*> threadWallet;
#define pwalletMain (*threadWallet.get())

extern Object JSONRPCError(int code, const string& message);

class CTxDump
{
public:
    CBlockIndex *pindex;
    int64 nValue;
    bool fSpent;
    CWalletTx* ptx;
    int nOut;
    CTxDump(CWalletTx* ptx = NULL, int nOut = -1)
    {
        pindex = NULL;
        nValue = 0;
        fSpent = false;
        this->ptx = ptx;
        this->nOut = nOut;
    }
};

Value importprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "importprivkey <privkey> [label]\n"
            "Adds a private key (as returned by dumpprivkey) to your wallet.");

    string strSecret = params[0].get_str();
    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret, pwalletMain->Unit());

    if (!fGood) throw JSONRPCError(-5,"Invalid private key");
    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (pwalletMain->fWalletUnlockMintOnly) // ppcoin: no importprivkey in mint-only mode
        throw JSONRPCError(-102, "Wallet is unlocked for minting only.");

    CKey key;
    bool fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);
    CKeyID vchAddress = key.GetPubKey().GetID();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookName(vchAddress, strLabel);

        if (!pwalletMain->AddKey(key))
            throw JSONRPCError(-4,"Error adding key to wallet");

        pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
        pwalletMain->ReacceptWalletTransactions();
    }

    MainFrameRepaint();

    return Value::null;
}

Value dumpprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey <address>\n"
            "Reveals the private key corresponding to <address>.");

    string strAddress = params[0].get_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        throw JSONRPCError(-5, "Invalid address");
    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (pwalletMain->fWalletUnlockMintOnly) // ppcoin: no dumpprivkey in mint-only mode
        throw JSONRPCError(-102, "Wallet is unlocked for minting only.");
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(-3, "Address does not refer to a key");
    CSecret vchSecret;
    bool fCompressed;
    if (!pwalletMain->GetSecret(keyID, vchSecret, fCompressed))
        throw JSONRPCError(-4,"Private key for address " + strAddress + " is not known");
    return CBitcoinSecret(vchSecret, fCompressed, pwalletMain->Unit()).ToString();
}

Value exportdividendkeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "exportdividendkeys\n"
            "Add the Bitcoin keys associated with the BlockShares addresses to the Bitcoin wallet. Bitcoin must be running and accept RPC commands.");

    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (pwalletMain->fWalletUnlockMintOnly) // ppcoin: no dumpprivkey in mint-only mode
        throw JSONRPCError(-102, "Wallet is unlocked for minting only.");

    Object ret;
    int nExportedCount, nErrorCount;
    pwalletMain->ExportDividendKeys(nExportedCount, nErrorCount);
    ret.push_back(Pair("exported", nExportedCount));
    ret.push_back(Pair("failed", nErrorCount));
    return ret;
}

Value dumpdividendkeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "dumpdividendkeys\n"
            "Returns an array of Bitcoin private keys associated with the BlockShare addresses of the wallet.");

    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (pwalletMain->fWalletUnlockMintOnly) // ppcoin: no dumpprivkey in mint-only mode
        throw JSONRPCError(-102, "Wallet is unlocked for minting only.");

    Array ret;
    vector<CDividendSecret> vSecret;
    pwalletMain->DumpDividendKeys(vSecret);

    BOOST_FOREACH(const CDividendSecret& secret, vSecret)
        ret.push_back(secret.ToString());

    return ret;
}

Value importnusharewallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importnusharewallet <NuShares wallet file> [walletpassword] [rescan=true]\n"
            "Import NuShares walletS.dat\n"
            "Password is only required if wallet is encrypted\n"
        );

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    if (pwalletMain->fWalletUnlockMintOnly) // ppcoin: no dumpprivkey in mint-only mode
        throw JSONRPCError(-102, "Wallet is unlocked for minting only.");
    if (pwalletMain->Unit() != '8')
        throw JSONRPCError(-12, "Error: Can only import to a BlockShares wallet");

    bool fFirstRun = false;

    if (fDebug)
        printf("Importing wallet %s\n", params[0].get_str().c_str());

    CWallet *pwalletImport = new CWallet(params[0].get_str().c_str());
    int nLoadWalletRet = pwalletImport->LoadWalletImport();

    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            throw JSONRPCError(-4, "Error importing wallet: Wallet corrupted");
        else if (nLoadWalletRet == DB_LOAD_FAIL)
            throw JSONRPCError(-4, "Wallet failed to load");
        else if (nLoadWalletRet == DB_INCORRECT_UNIT)
            throw JSONRPCError(-4, "Unsupported wallet unit");
        else if (fDebug)
            printf("Non fatal errors occurred while importing wallet\n");
    }

    // Handle encrypted wallets. Wallets first need to be unlocked before the keys
    // can be added into your clam wallet.
    if (pwalletImport->IsCrypted() && pwalletImport->IsLocked()) {
        bool fGotWalletPass = true;
        if (params.size() < 2)
            fGotWalletPass = false;
        else
        {
            // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
            // Alternately, find a way to make params[0] mlock()'d to begin with.
            SecureString strWalletPass;
            strWalletPass.reserve(100);
            strWalletPass = params[1].get_str().c_str();
            if (strWalletPass.length() > 0)
            {
                if (!pwalletImport->Unlock(strWalletPass))
                    throw JSONRPCError(-14, "Error: The wallet passphrase entered was incorrect for the wallet you are attempting to import.");
            } else
                fGotWalletPass = false;
        }

        if (!fGotWalletPass)
            throw runtime_error(
                "importnusharewallet <NuShares wallet file> [walletpassword] [rescan=true]\n"
                "Import NuShares walletS.dat\n"
                "You are attempting to import an encrypted wallet\n"
                "The passphrase must be entered to import the wallet\n"
                );
    }

    std::string strLabel = "Converted NuShares";
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        LOCK(pwalletImport->cs_wallet);

        // Import private keys
        std::set<CKeyID> setKeys;
        pwalletImport->GetKeys(setKeys);

        BOOST_FOREACH(const CKeyID &keyid, setKeys) {
            std::string strAddr = CBitcoinAddress(keyid, '8').ToString();

            CKey key;
            if (pwalletImport->GetKey(keyid, key)) {

                if (pwalletMain->HaveKey(keyid)) {
                    printf("Skipping address %s (key already present)\n", strAddr.c_str());
                    continue;
                }

                printf("Importing key for address %s\n", strAddr.c_str());

                pwalletMain->AddKey(key);
                pwalletMain->SetAddressBookName(keyid, strLabel);
            }
        }

        // Import P2SH scripts
        std::set<CScriptID> setHashes;
        pwalletImport->GetCScripts(setHashes);

        BOOST_FOREACH(const CScriptID &hash, setHashes) {
            std::string strAddr = CBitcoinAddress(hash, '8').ToString();

            CScript script;
            if (pwalletImport->GetCScript(hash, script)) {

                if (pwalletMain->HaveCScript(hash)) {
                    printf("Skipping P2SH address %s (already present)\n", strAddr.c_str());
                    continue;
                }

                printf("Importing script for P2SH address %s\n", strAddr.c_str());

                pwalletMain->AddCScript(script);
                pwalletMain->SetAddressBookName(hash, strLabel);
            }
        }
    }

    // Clean up unregistered wallet
    UnregisterWallet(pwalletImport);
    delete pwalletImport;

    pwalletMain->MarkDirty();

    if (fRescan)
    {
        if (fDebug) printf("Scanning for available BKS...\n");
        pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
        pwalletMain->ReacceptWalletTransactions();
    }

    if (fDebug) printf("Wallet import complete\n");

    MainFrameRepaint();

    return Value::null;
}
