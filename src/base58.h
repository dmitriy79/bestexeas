// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin Developers
// Copyright (c) 2011-2012 The PPCoin developers
// Copyright (c) 2013-2014 Peershares Developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


//
// Why base-58 instead of standard base-64 encoding?
// - Don't want 0OIl characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - A string with non-alphanumeric characters is not as easily accepted as an account number.
// - E-mail usually won't line-break if there's no punctuation to break at.
// - Doubleclicking selects the whole number as one word if it's all alphanumeric.
//
#ifndef BITCOIN_BASE58_H
#define BITCOIN_BASE58_H

#include <string>
#include <vector>

#include "bignum.h"
#include "key.h"
#include <boost/format.hpp>
#include "script.h"
#include "allocators.h"

static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Encode a byte sequence as a base58-encoded string
inline std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend)
{
    CAutoBN_CTX pctx;
    CBigNum bn58 = 58;
    CBigNum bn0 = 0;

    // Convert big endian data to little endian
    // Extra zero at the end make sure bignum will interpret as a positive number
    std::vector<unsigned char> vchTmp(pend-pbegin+1, 0);
    reverse_copy(pbegin, pend, vchTmp.begin());

    // Convert little endian data to bignum
    CBigNum bn;
    bn.setvch(vchTmp);

    // Convert bignum to std::string
    std::string str;
    // Expected size increase from base58 conversion is approximately 137%
    // use 138% to be safe
    str.reserve((pend - pbegin) * 138 / 100 + 1);
    CBigNum dv;
    CBigNum rem;
    while (bn > bn0)
    {
        if (!BN_div(&dv, &rem, &bn, &bn58, pctx))
            throw bignum_error("EncodeBase58 : BN_div failed");
        bn = dv;
        unsigned int c = rem.getulong();
        str += pszBase58[c];
    }

    // Leading zeroes encoded as base58 zeros
    for (const unsigned char* p = pbegin; p < pend && *p == 0; p++)
        str += pszBase58[0];

    // Convert little endian std::string to big endian
    reverse(str.begin(), str.end());
    return str;
}

// Encode a byte vector as a base58-encoded string
inline std::string EncodeBase58(const std::vector<unsigned char>& vch)
{
    return EncodeBase58(&vch[0], &vch[0] + vch.size());
}

// Decode a base58-encoded string psz into byte vector vchRet
// returns true if decoding is succesful
inline bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet)
{
    CAutoBN_CTX pctx;
    vchRet.clear();
    CBigNum bn58 = 58;
    CBigNum bn = 0;
    CBigNum bnChar;
    while (isspace(*psz))
        psz++;

    // Convert big endian string to bignum
    for (const char* p = psz; *p; p++)
    {
        const char* p1 = strchr(pszBase58, *p);
        if (p1 == NULL)
        {
            while (isspace(*p))
                p++;
            if (*p != '\0')
                return false;
            break;
        }
        bnChar.setulong(p1 - pszBase58);
        if (!BN_mul(&bn, &bn, &bn58, pctx))
            throw bignum_error("DecodeBase58 : BN_mul failed");
        bn += bnChar;
    }

    // Get bignum as little endian data
    std::vector<unsigned char> vchTmp = bn.getvch();

    // Trim off sign byte if present
    if (vchTmp.size() >= 2 && vchTmp.end()[-1] == 0 && vchTmp.end()[-2] >= 0x80)
        vchTmp.erase(vchTmp.end()-1);

    // Restore leading zeros
    int nLeadingZeros = 0;
    for (const char* p = psz; *p == pszBase58[0]; p++)
        nLeadingZeros++;
    vchRet.assign(nLeadingZeros + vchTmp.size(), 0);

    // Convert little endian data to big endian
    reverse_copy(vchTmp.begin(), vchTmp.end(), vchRet.end() - vchTmp.size());
    return true;
}

// Decode a base58-encoded string str into byte vector vchRet
// returns true if decoding is succesful
inline bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58(str.c_str(), vchRet);
}




// Encode a byte vector to a base58-encoded string, including checksum
inline std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn)
{
    // add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return EncodeBase58(vch);
}

// Decode a base58-encoded string psz that includes a checksum, into byte vector vchRet
// returns true if decoding is succesful
inline bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet))
        return false;
    if (vchRet.size() < 4)
    {
        vchRet.clear();
        return false;
    }
    uint256 hash = Hash(vchRet.begin(), vchRet.end()-4);
    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0)
    {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size()-4);
    return true;
}

// Decode a base58-encoded string str that includes a checksum, into byte vector vchRet
// returns true if decoding is succesful
inline bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58Check(str.c_str(), vchRet);
}





/** Base class for all base58-encoded data */
class CBase58Data
{
protected:
    // the version byte
    unsigned char nVersion;

    // the actually encoded data
    typedef std::vector<unsigned char, zero_after_free_allocator<unsigned char> > vector_uchar;
    vector_uchar vchData;

    CBase58Data()
    {
        nVersion = 0;
        vchData.clear();
    }

    void SetData(int nVersionIn, const void* pdata, size_t nSize)
    {
        nVersion = nVersionIn;
        vchData.resize(nSize);
        if (!vchData.empty())
            memcpy(&vchData[0], pdata, nSize);
    }

    void SetData(int nVersionIn, const unsigned char *pbegin, const unsigned char *pend)
    {
        SetData(nVersionIn, (void*)pbegin, pend - pbegin);
    }

public:
    bool SetString(const char* psz)
    {
        std::vector<unsigned char> vchTemp;
        DecodeBase58Check(psz, vchTemp);
        if (vchTemp.empty())
        {
            vchData.clear();
            nVersion = 0;
            return false;
        }
        nVersion = vchTemp[0];
        vchData.resize(vchTemp.size() - 1);
        if (!vchData.empty())
            memcpy(&vchData[0], &vchTemp[1], vchData.size());
        OPENSSL_cleanse(&vchTemp[0], vchTemp.size());
        return true;
    }

    bool SetString(const std::string& str)
    {
        return SetString(str.c_str());
    }

    std::string ToString() const
    {
        std::vector<unsigned char> vch(1, nVersion);
        vch.insert(vch.end(), vchData.begin(), vchData.end());
        return EncodeBase58Check(vch);
    }

    int CompareTo(const CBase58Data& b58) const
    {
        if (nVersion < b58.nVersion) return -1;
        if (nVersion > b58.nVersion) return  1;
        if (vchData < b58.vchData)   return -1;
        if (vchData > b58.vchData)   return  1;
        return 0;
    }

    bool operator==(const CBase58Data& b58) const { return CompareTo(b58) == 0; }
    bool operator!=(const CBase58Data& b58) const { return CompareTo(b58) != 0; }
    bool operator<=(const CBase58Data& b58) const { return CompareTo(b58) <= 0; }
    bool operator>=(const CBase58Data& b58) const { return CompareTo(b58) >= 0; }
    bool operator< (const CBase58Data& b58) const { return CompareTo(b58) <  0; }
    bool operator> (const CBase58Data& b58) const { return CompareTo(b58) >  0; }
};

/** base58-encoded bitcoin addresses.
 * Public-key-hash-addresses have version 55 (or 111 testnet).
 * The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
 * Script-hash-addresses have version 117 (or 196 testnet).
 * The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
 */
class CBitcoinAddress;
class CBitcoinAddressVisitor : public boost::static_visitor<bool>
{
private:
    CBitcoinAddress *addr;
    unsigned char cUnit;
public:
    CBitcoinAddressVisitor(CBitcoinAddress *addrIn, unsigned char cUnit) : addr(addrIn), cUnit(cUnit) { }
    bool operator()(const CKeyID &id) const;
    bool operator()(const CScriptID &id) const;
    bool operator()(const CNoDestination &no) const;
};

class CBitcoinAddress : public CBase58Data
{
public:
    enum
    {
        EIGHT_PUBKEY_ADDRESS = 18,
        EIGHT_SCRIPT_ADDRESS = 20,
        EIGHT_PUBKEY_ADDRESS_TEST = 127,
        EIGHT_SCRIPT_ADDRESS_TEST = 130,

        C_PUBKEY_ADDRESS = 28,
        C_SCRIPT_ADDRESS = 30,
        C_PUBKEY_ADDRESS_TEST = 87,
        C_SCRIPT_ADDRESS_TEST = 90,
    };

    unsigned char PubKeyVersion(unsigned char cUnit) const
    {
        switch (cUnit)
        {
            case '8':
                return fTestNet ? EIGHT_PUBKEY_ADDRESS_TEST : EIGHT_PUBKEY_ADDRESS;
            case 'C':
                return fTestNet ? C_PUBKEY_ADDRESS_TEST : C_PUBKEY_ADDRESS;
            default:
                throw std::runtime_error((boost::format("No address version defined for unit '%c'") % cUnit).str());
        }
    }

    unsigned char ScriptVersion(unsigned char cUnit) const
    {
        switch (cUnit)
        {
            case '8':
                return fTestNet ? EIGHT_SCRIPT_ADDRESS_TEST : EIGHT_SCRIPT_ADDRESS;
            case 'C':
                return fTestNet ? C_SCRIPT_ADDRESS_TEST : C_SCRIPT_ADDRESS;
            default:
                throw std::runtime_error((boost::format("No address version defined for unit '%c'") % cUnit).str());
        }
    }

    unsigned char GetUnit() const
    {
        if (fTestNet)
        {
            switch (nVersion)
            {
                case EIGHT_PUBKEY_ADDRESS_TEST:
                case EIGHT_SCRIPT_ADDRESS_TEST:
                    return '8';
                case C_PUBKEY_ADDRESS_TEST:
                case C_SCRIPT_ADDRESS_TEST:
                    return 'C';
                default:
                    return '?';
            }
        }
        else
        {
            switch (nVersion)
            {
                case EIGHT_PUBKEY_ADDRESS:
                case EIGHT_SCRIPT_ADDRESS:
                    return '8';
                case C_PUBKEY_ADDRESS:
                case C_SCRIPT_ADDRESS:
                    return 'C';
                default:
                    return '?';
            }
        }
    }

    bool Set(const CKeyID &id, unsigned char cUnit) {
        SetData(PubKeyVersion(cUnit), &id, 20);
        return true;
    }

    bool Set(const CScriptID &id, unsigned char cUnit) {
        SetData(ScriptVersion(cUnit), &id, 20);
        return true;
    }

    bool Set(const CTxDestination &dest, unsigned char cUnit)
    {
        return boost::apply_visitor(CBitcoinAddressVisitor(this, cUnit), dest);
    }

    bool IsValid(unsigned char cUnit) const
    {
        if (nVersion != PubKeyVersion(cUnit) && nVersion != ScriptVersion(cUnit))
            return false;

        return vchData.size() == 20;
    }
    bool IsValid() const
    {
        unsigned char cUnit = GetUnit();

        if (cUnit == '?')
            return false;

        return IsValid(cUnit);
    }

    CBitcoinAddress()
    {
    }

    explicit CBitcoinAddress(const CTxDestination &dest, unsigned char cUnit)
    {
        Set(dest, cUnit);
    }

    explicit CBitcoinAddress(const std::string& strAddress)
    {
        SetString(strAddress);
    }

    explicit CBitcoinAddress(const char* pszAddress)
    {
        SetString(pszAddress);
    }

    CTxDestination Get() const {
        unsigned char cUnit = GetUnit();

        if (cUnit == '?')
            return CNoDestination();

        if (!IsValid(cUnit))
            return CNoDestination();

        uint160 id;
        memcpy(&id, &vchData[0], 20);

        if (IsScript(cUnit))
            return CScriptID(id);
        else
            return CKeyID(id);
    }

    bool GetKeyID(CKeyID &keyID) const {
        unsigned char cUnit = GetUnit();

        if (!IsValid(cUnit))
            return false;

        if (IsScript(cUnit))
            return false;

        uint160 id;
        memcpy(&id, &vchData[0], 20);
        keyID = CKeyID(id);
        return true;
    }

    bool IsScript(unsigned char cUnit) const {
        return nVersion == ScriptVersion(cUnit);
    }

    bool IsScript() const {
        unsigned char cUnit = GetUnit();

        if (cUnit == '?')
            return false;

        return IsScript(cUnit);
    }
};

bool inline CBitcoinAddressVisitor::operator()(const CKeyID &id) const         { return addr->Set(id, cUnit); }
bool inline CBitcoinAddressVisitor::operator()(const CScriptID &id) const      { return addr->Set(id, cUnit); }
bool inline CBitcoinAddressVisitor::operator()(const CNoDestination &id) const { return false; }

class CDividendAddress;
class CDividendAddressVisitor : public boost::static_visitor<bool>
{
private:
    CDividendAddress *addr;
public:
    CDividendAddressVisitor(CDividendAddress *addrIn) : addr(addrIn) { }
    bool operator()(const CKeyID &id) const;
    bool operator()(const CScriptID &id) const;
    bool operator()(const CNoDestination &no) const;
};

class CDividendAddress : public CBitcoinAddress
{
public:
    enum
    {
        PUBKEY_ADDRESS = 0,
        SCRIPT_ADDRESS = 5,
        PUBKEY_ADDRESS_TEST = 111,
        SCRIPT_ADDRESS_TEST = 196,
    };

    bool Set(const CKeyID &id) {
        SetData(fTestNet ? PUBKEY_ADDRESS_TEST : PUBKEY_ADDRESS, &id, 20);
        return true;
    }

    bool Set(const CScriptID &id) {
        SetData(fTestNet ? SCRIPT_ADDRESS_TEST : SCRIPT_ADDRESS, &id, 20);
        return true;
    }

    bool Set(const CTxDestination &dest)
    {
        return boost::apply_visitor(CDividendAddressVisitor(this), dest);
    }

    bool IsValid() const
    {
        unsigned int nExpectedSize = 20;
        bool fExpectTestNet = false;
        switch(nVersion)
        {
            case PUBKEY_ADDRESS:
                nExpectedSize = 20; // Hash of public key
                fExpectTestNet = false;
                break;
            case SCRIPT_ADDRESS:
                nExpectedSize = 20; // Hash of CScript
                fExpectTestNet = false;
                break;

            case PUBKEY_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;
            case SCRIPT_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;

            default:
                return false;
        }
        return fExpectTestNet == fTestNet && vchData.size() == nExpectedSize;
    }

    CDividendAddress()
    {
    }

    CDividendAddress(const CBitcoinAddress &addr)
    {
        Set(addr.Get());
    }

    CDividendAddress(const CTxDestination &dest)
    {
        Set(dest);
    }

    CDividendAddress(const std::string& strAddress)
    {
        SetString(strAddress);
    }

    CDividendAddress(const char* pszAddress)
    {
        SetString(pszAddress);
    }

    CTxDestination Get() const {
        if (!IsValid())
            return CNoDestination();
        switch (nVersion) {
        case PUBKEY_ADDRESS:
        case PUBKEY_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            return CKeyID(id);
        }
        case SCRIPT_ADDRESS:
        case SCRIPT_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            return CScriptID(id);
        }
        }
        return CNoDestination();
    }

    bool GetKeyID(CKeyID &keyID) const {
        if (!IsValid())
            return false;
        switch (nVersion) {
        case PUBKEY_ADDRESS:
        case PUBKEY_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            keyID = CKeyID(id);
            return true;
        }
        default: return false;
        }
    }

    bool IsScript() const {
        if (!IsValid())
            return false;
        switch (nVersion) {
        case SCRIPT_ADDRESS:
        case SCRIPT_ADDRESS_TEST: {
            return true;
        }
        default: return false;
        }
    }
};

bool inline CDividendAddressVisitor::operator()(const CKeyID &id) const         { return addr->Set(id); }
bool inline CDividendAddressVisitor::operator()(const CScriptID &id) const      { return addr->Set(id); }
bool inline CDividendAddressVisitor::operator()(const CNoDestination &id) const { return false; }

/** A base58-encoded secret key */
class CBitcoinSecret : public CBase58Data
{
public:
    enum
    {
        BLOCKSHARES_SECRET = 153,      // PXXXXXXXX... for compressed keys
        BLOCKSHARES_SECRET_TEST = 227, // aXXXXXXXX... for compressed keys
        BLOCKCREDITS_SECRET = 154,        // PXXXXXXXX... for compressed keys
        BLOCKCREDITS_SECRET_TEST = 228,   // aXXXXXXXX... for compressed keys
    };

    void SetSecret(const CSecret& vchSecret, bool fCompressed, unsigned char cUnit)
    { 
        assert(vchSecret.size() == 32);
        SetData(ExpectedVersion(cUnit), &vchSecret[0], vchSecret.size());
        if (fCompressed)
            vchData.push_back(1);
    }

    CSecret GetSecret(bool &fCompressedOut)
    {
        CSecret vchSecret;
        vchSecret.resize(32);
        memcpy(&vchSecret[0], &vchData[0], 32);
        fCompressedOut = vchData.size() == 33;
        return vchSecret;
    }

    unsigned char ExpectedVersion(unsigned char cUnit, bool fTest = fTestNet) const
    {
        switch (cUnit)
        {
            case '8':
                if (fTest)
                    return BLOCKSHARES_SECRET_TEST;
                else
                    return BLOCKSHARES_SECRET;
            case 'C':
                if (fTest)
                    return BLOCKCREDITS_SECRET_TEST;
                else
                    return BLOCKCREDITS_SECRET;
            default:
                throw std::runtime_error("Unknown unit");
        }
    }

    bool IsValid(unsigned char cUnit) const
    {
        bool fExpectTestNet = false;
        unsigned char cDetectedUnit = '?';
        switch(nVersion)
        {
            case BLOCKSHARES_SECRET:
                cDetectedUnit = '8';
                break;
            case BLOCKSHARES_SECRET_TEST:
                cDetectedUnit = '8';
                fExpectTestNet = true;
                break;

            case BLOCKCREDITS_SECRET:
                cDetectedUnit = 'C';
                break;
            case BLOCKCREDITS_SECRET_TEST:
                cDetectedUnit = 'C';
                fExpectTestNet = true;
                break;

            default:
                return false;
        }

        bool fCorrectNetwork = fExpectTestNet == fTestNet;
        bool fExpectedFormat = vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
        bool fCorrectUnit = cDetectedUnit == cUnit;

        return fCorrectNetwork && fExpectedFormat && fCorrectUnit;
    }

    bool SetString(const char* pszSecret, unsigned char cUnit)
    {
        return CBase58Data::SetString(pszSecret) && IsValid(cUnit);
    }

    bool SetString(const std::string& strSecret, unsigned char cUnit)
    {
        return SetString(strSecret.c_str(), cUnit);
    }

    CBitcoinSecret(const CSecret& vchSecret, bool fCompressed, unsigned char cUnit)
    {
        SetSecret(vchSecret, fCompressed, cUnit);
    }

    CBitcoinSecret()
    {
    }
};

class CDividendSecret : public CBase58Data
{
public:
    void SetSecret(const CSecret& vchSecret, bool fCompressed)
    {
        assert(vchSecret.size() == 32);
        SetData(128 + (fTestNet ? CDividendAddress::PUBKEY_ADDRESS_TEST : CDividendAddress::PUBKEY_ADDRESS), &vchSecret[0], vchSecret.size());
        if (fCompressed)
            vchData.push_back(1);
    }

    CSecret GetSecret(bool &fCompressedOut)
    {
        CSecret vchSecret;
        vchSecret.resize(32);
        memcpy(&vchSecret[0], &vchData[0], 32);
        fCompressedOut = vchData.size() == 33;
        return vchSecret;
    }

    bool IsValid() const
    {
        bool fExpectTestNet = false;
        switch(nVersion)
        {
             case (128 + CDividendAddress::PUBKEY_ADDRESS):
                break;

            case (128 + CDividendAddress::PUBKEY_ADDRESS_TEST):
                fExpectTestNet = true;
                break;

            default:
                return false;
        }
        return fExpectTestNet == fTestNet && (vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1));
    }

    bool SetString(const char* pszSecret)
    {
        return CBase58Data::SetString(pszSecret) && IsValid();
    }

    bool SetString(const std::string& strSecret)
    {
        return SetString(strSecret.c_str());
    }

    CDividendSecret(const CSecret& vchSecret, bool fCompressed)
    {
        SetSecret(vchSecret, fCompressed);
    }

    CDividendSecret()
    {
    }
};
#endif
