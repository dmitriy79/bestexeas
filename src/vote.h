// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTE_H
#define VOTE_H

#include "serialize.h"
#include "base58.h"
#include "exchange.h"
#include "util.h"

class CBlock;
class CBlockIndex;

static const int64 COIN_PARK_RATE = 100000 * COIN; // Park rate internal encoding precision. The minimum possible rate is (1.0 / COIN_PARK_RATE) coins per parked coin
static const int64 MAX_PARK_RATE = 1000000 * COIN_PARK_RATE;
static const unsigned char MAX_COMPACT_DURATION = 30; // about 2000 years
static const int64 MAX_PARK_DURATION = 1000000000; // about 1900 years

// Confirmations is a unsigned short
static const int MAX_CONFIRMATIONS = 65535;

// Signers is a 8bit uint
static const int MAX_SIGNERS = 255;

// Normally the minimum M-of-N signers is 2-of-3 but we keep open the possibility for non-multisig blockchains
static const int MIN_REQ_SIGNERS = 1;
static const int MIN_TOTAL_SIGNERS = 1;

class CCustodianVote
{
public:
    unsigned char cUnit;
    bool fScript;
    uint160 hashAddress;
    int64 nAmount;

    CCustodianVote() :
        cUnit('?'),
        fScript(false),
        hashAddress(0),
        nAmount(0)
    {
    }

    bool IsValid(int nProtocolVersion) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(cUnit);
        if (nVersion >= 20200) // version 0.2.2
            READWRITE(fScript);
        else if (fRead)
            const_cast<CCustodianVote*>(this)->fScript = false;
        READWRITE(hashAddress);
        READWRITE(nAmount);
    )

    inline bool operator==(const CCustodianVote& other) const
    {
        return (cUnit == other.cUnit &&
                fScript == other.fScript &&
                hashAddress == other.hashAddress &&
                nAmount == other.nAmount);
    }
    inline bool operator!=(const CCustodianVote& other) const
    {
        return !(*this == other);
    }

    class CDestinationVisitor : public boost::static_visitor<bool>
    {
        private:
            CCustodianVote *custodianVote;
        public:
            CDestinationVisitor(CCustodianVote *custodianVote) : custodianVote(custodianVote) { }

            bool operator()(const CNoDestination &dest) const {
                custodianVote->fScript = false;
                custodianVote->hashAddress = 0;
                return false;
            }

            bool operator()(const CKeyID &keyID) const {
                custodianVote->fScript = false;
                custodianVote->hashAddress = keyID;
                return true;
            }

            bool operator()(const CScriptID &scriptID) const {
                custodianVote->fScript = true;
                custodianVote->hashAddress = scriptID;
                return true;
            }
    };

    void SetAddress(const CBitcoinAddress& address)
    {
        cUnit = address.GetUnit();
        CTxDestination destination = address.Get();
        boost::apply_visitor(CDestinationVisitor(this), destination);
    }

    CBitcoinAddress GetAddress() const
    {
        CBitcoinAddress address;
        if (fScript)
            address.Set(CScriptID(hashAddress), cUnit);
        else
            address.Set(CKeyID(hashAddress), cUnit);
        return address;
    }

    bool operator< (const CCustodianVote& other) const
    {
        if (cUnit < other.cUnit)
            return true;
        if (cUnit > other.cUnit)
            return false;
        if (nAmount < other.nAmount)
            return true;
        if (nAmount > other.nAmount)
            return false;
        if (fScript < other.fScript)
            return true;
        if (fScript > other.fScript)
            return false;
        if (hashAddress < other.hashAddress)
            return true;
        return false;
    }
};

inline bool CompactDurationRange(unsigned char nCompactDuration)
{
    return (nCompactDuration < MAX_COMPACT_DURATION);
}

inline bool ParkDurationRange(int64 nDuration)
{
    return (nDuration >= 1 && nDuration <= MAX_PARK_DURATION);
}

inline bool ParkRateRange(int64 nRate)
{
    return (nRate >= 0 && nRate <= MAX_PARK_RATE);
}

inline int64 CompactDurationToDuration(unsigned char nCompactDuration)
{
    return 1 << nCompactDuration;
}

class CParkRate
{
public:
    unsigned char nCompactDuration;
    int64 nRate;

    CParkRate() :
        nCompactDuration(0),
        nRate(0)
    {
    }

    CParkRate(unsigned char nCompactDuration, int64 nRate) :
        nCompactDuration(nCompactDuration),
        nRate(nRate)
    {
    }

    bool IsValid() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nCompactDuration);
        READWRITE(nRate);
    )

    int64 GetDuration() const
    {
        if (!CompactDurationRange(nCompactDuration))
            throw std::runtime_error("Park rate compact duration out of range");
        return CompactDurationToDuration(nCompactDuration);
    }

    friend bool operator==(const CParkRate& a, const CParkRate& b)
    {
        return (a.nCompactDuration == b.nCompactDuration &&
                a.nRate == b.nRate);
    }

    bool operator<(const CParkRate& other) const
    {
        if (nCompactDuration < other.nCompactDuration)
            return true;
        if (nCompactDuration > other.nCompactDuration)
            return false;
        return nRate < other.nRate;
    }
};

class CParkRateVote
{
public:
    unsigned char cUnit;
    std::vector<CParkRate> vParkRate;

    CParkRateVote() :
        cUnit('?')
    {
    }

    void SetNull()
    {
        cUnit = '?';
        vParkRate.clear();
    }

    bool IsValid() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(cUnit);
        READWRITE(vParkRate);
    )

    CScript ToParkRateResultScript() const;

    friend bool operator==(const CParkRateVote& a, const CParkRateVote& b)
    {
        return (a.cUnit     == b.cUnit &&
                a.vParkRate == b.vParkRate);
    }

    std::string ToString() const;
};

class CReputationVote
{
public:
    bool fScript;
    uint160 hashAddress;
    signed char nWeight;

    CReputationVote() :
        fScript(false),
        hashAddress(0),
        nWeight(0)
    {
    }

    CReputationVote(const CBitcoinAddress& address, char nWeight) :
        nWeight(nWeight)
    {
        SetAddress(address);
    }

    bool IsValid(int nProtocolVersion) const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(fScript);
        READWRITE(hashAddress);
        READWRITE(nWeight);
    )

    inline bool operator==(const CReputationVote& other) const
    {
        return (fScript == other.fScript &&
                hashAddress == other.hashAddress &&
                nWeight == other.nWeight);
    }
    inline bool operator!=(const CReputationVote& other) const
    {
        return !(*this == other);
    }

    class CDestinationVisitor : public boost::static_visitor<bool>
    {
        private:
            CReputationVote *reputationVote;
        public:
            CDestinationVisitor(CReputationVote *reputationVote) : reputationVote(reputationVote) { }

            bool operator()(const CNoDestination &dest) const {
                reputationVote->fScript = false;
                reputationVote->hashAddress = 0;
                return false;
            }

            bool operator()(const CKeyID &keyID) const {
                reputationVote->fScript = false;
                reputationVote->hashAddress = keyID;
                return true;
            }

            bool operator()(const CScriptID &scriptID) const {
                reputationVote->fScript = true;
                reputationVote->hashAddress = scriptID;
                return true;
            }
    };

    void SetDestination(const CTxDestination& destination)
    {
        boost::apply_visitor(CDestinationVisitor(this), destination);
    }

    CTxDestination GetDestination() const
    {
        if (fScript)
            return CScriptID(hashAddress);
        else
            return CKeyID(hashAddress);
    }

    void SetAddress(const CBitcoinAddress& address)
    {
        if (address.GetUnit() != '8')
            throw std::runtime_error("Invalid unit on reputation vote");
        SetDestination(address.Get());
    }

    CBitcoinAddress GetAddress() const
    {
        CBitcoinAddress address;
        address.Set(GetDestination(), '8');
        return address;
    }

    bool operator< (const CReputationVote& other) const
    {
        if (nWeight < other.nWeight)
            return true;
        if (nWeight > other.nWeight)
            return false;
        if (fScript < other.fScript)
            return true;
        if (fScript > other.fScript)
            return false;
        if (hashAddress < other.hashAddress)
            return true;
        return false;
    }
};

class CSignerRewardVote
{
public:
    // A negative value means no vote, i.e. a vote to keep the current value
    int16_t nCount;
    int32_t nAmount;

    CSignerRewardVote() :
        nCount(-1),
        nAmount(-1)
    {
    }

    void SetNull()
    {
        nCount = -1;
        nAmount = -1;
    }

    void Set(int nCount, int64 nAmount)
    {
        if (nCount < std::numeric_limits<int16_t>::min() || nCount > std::numeric_limits<int16_t>::max())
            throw std::runtime_error("Signer reward count out of range");
        if (nAmount < std::numeric_limits<int32_t>::min() || nAmount > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("Signer reward amount out of range");
        this->nCount = nCount;
        this->nAmount = nAmount;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nCount);
        READWRITE(nAmount);
    )

    bool IsValid(int nProtocolVersion) const
    {
        return true;
    }

    bool IsValidInBlock(int nProtocolVersion) const
    {
        if (nProtocolVersion < PROTOCOL_V4_0 && (nCount > 0 || nAmount > 0))
            return false;
        return true;
    }

    inline bool operator==(const CSignerRewardVote& other) const
    {
        return (nCount == other.nCount &&
                nAmount == other.nAmount);
    }
    inline bool operator!=(const CSignerRewardVote& other) const
    {
        return !(*this == other);
    }
};

class CAssetVote
{
public:
    uint32_t nAssetId;
    uint16_t nNumberOfConfirmations;
    uint8_t nRequiredDepositSigners;
    uint8_t nTotalDepositSigners;
    uint8_t nMaxTradeExpParam;
    uint8_t nMinTradeExpParam;
    uint8_t nUnitExponent;

    CAssetVote() :
        nAssetId(0),
        nNumberOfConfirmations(0),
        nRequiredDepositSigners(0),
        nTotalDepositSigners(0),
        nMaxTradeExpParam(0),
        nMinTradeExpParam(0),
        nUnitExponent(0)
    {
    }

    bool IsValid(int nProtocolVersion) const
    {
        return (IsValidAssetId(nAssetId) &&
                nNumberOfConfirmations > 0 &&
                nRequiredDepositSigners >= MIN_REQ_SIGNERS &&
                nTotalDepositSigners >= MIN_TOTAL_SIGNERS &&
                nRequiredDepositSigners < nTotalDepositSigners &&
                nMaxTradeExpParam <= EXP_SERIES_MAX_PARAM &&
                nMinTradeExpParam <= nMaxTradeExpParam &&
                nUnitExponent <= MAX_TRADABLE_UNIT_EXPONENT);
    }

    inline int64 GetMaxTrade() const
    {
        return nMaxTradeExpParam <= EXP_SERIES_MAX_PARAM ? pnExponentialSeries[nMaxTradeExpParam] : 0;
    }

    inline int64 GetMinTrade() const
    {
        return nMinTradeExpParam <= EXP_SERIES_MAX_PARAM ? pnExponentialSeries[nMinTradeExpParam] : 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nAssetId);
        READWRITE(nNumberOfConfirmations);
        READWRITE(nRequiredDepositSigners);
        READWRITE(nTotalDepositSigners);
        READWRITE(nMaxTradeExpParam);
        READWRITE(nMinTradeExpParam);
        READWRITE(nUnitExponent);
    )

    inline bool operator==(const CAssetVote& other) const
    {
        return (nAssetId == other.nAssetId &&
                nNumberOfConfirmations == other.nNumberOfConfirmations &&
                nRequiredDepositSigners == other.nRequiredDepositSigners &&
                nTotalDepositSigners == other.nTotalDepositSigners &&
                nMaxTradeExpParam == other.nMaxTradeExpParam &&
                nMinTradeExpParam == other.nMinTradeExpParam &&
                nUnitExponent == other.nUnitExponent);
    }
    inline bool operator!=(const CAssetVote& other) const
    {
        return !(*this == other);
    }
    bool operator< (const CAssetVote& other) const
    {
        if (nAssetId != other.nAssetId)
            return nAssetId < other.nAssetId;
        if (nNumberOfConfirmations != other.nNumberOfConfirmations)
            return nNumberOfConfirmations < other.nNumberOfConfirmations;
        if (nRequiredDepositSigners != other.nRequiredDepositSigners)
            return nRequiredDepositSigners < other.nRequiredDepositSigners;
        if (nTotalDepositSigners != other.nTotalDepositSigners)
            return nTotalDepositSigners < other.nTotalDepositSigners;
        if (nMaxTradeExpParam != other.nMaxTradeExpParam)
            return nMaxTradeExpParam < other.nMaxTradeExpParam;
        if (nMinTradeExpParam != other.nMinTradeExpParam)
            return nMinTradeExpParam < other.nMinTradeExpParam;
        if (nUnitExponent != other.nUnitExponent)
            return nUnitExponent < other.nUnitExponent;
        return false;
    }

    bool ProducesAsset(const CAsset& asset) const
    {
        return (nAssetId == asset.nAssetId &&
                nNumberOfConfirmations == asset.nNumberOfConfirmations &&
                nRequiredDepositSigners == asset.nRequiredDepositSigners &&
                nTotalDepositSigners == asset.nTotalDepositSigners &&
                GetMaxTrade() == asset.GetMaxTrade() &&
                GetMinTrade() == asset.GetMinTrade() &&
                nUnitExponent == asset.nUnitExponent);
    }
};

class CVote
{
public:
    int nVersionVote;
    std::vector<CCustodianVote> vCustodianVote;
    std::vector<CParkRateVote> vParkRateVote;
    std::vector<uint160> vMotion;
    std::map<unsigned char, uint32_t> mapFeeVote;
    std::vector<CReputationVote> vReputationVote;
    CSignerRewardVote signerReward;
    std::vector<CAssetVote> vAssetVote;

    int64 nCoinAgeDestroyed;

    CVote() :
        nVersionVote(PROTOCOL_VERSION),
        nCoinAgeDestroyed(0)
    {
    }

    void SetNull()
    {
        nVersionVote = 0;
        vCustodianVote.clear();
        vParkRateVote.clear();
        vMotion.clear();
        mapFeeVote.clear();
        vReputationVote.clear();
        vAssetVote.clear();
        nCoinAgeDestroyed = 0;
    }

    template<typename Stream>
    unsigned int ReadWriteSingleMotion(Stream& s, int nType, int nVersion, CSerActionGetSerializeSize ser_action) const
    {
        uint160 hashMotion;
        return ::SerReadWrite(s, hashMotion, nType, nVersion, ser_action);
    }

    template<typename Stream>
    unsigned int ReadWriteSingleMotion(Stream& s, int nType, int nVersion, CSerActionSerialize ser_action) const
    {
        uint160 hashMotion;
        switch (vMotion.size())
        {
            case 0:
                hashMotion = 0;
                break;
            case 1:
                hashMotion = vMotion[0];
                break;
            default:
                printf("Warning: serializing multiple motion votes in a vote structure not supporting it. Only the first motion is serialized.\n");
                hashMotion = vMotion[0];
                break;
        }
        return ::SerReadWrite(s, hashMotion, nType, nVersion, ser_action);
    }

    template<typename Stream>
    unsigned int ReadWriteSingleMotion(Stream& s, int nType, int nVersion, CSerActionUnserialize ser_action)
    {
        uint160 hashMotion;
        unsigned int result = ::SerReadWrite(s, hashMotion, nType, nVersion, ser_action);
        vMotion.clear();
        if (hashMotion != 0)
            vMotion.push_back(hashMotion);
        return result;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nVersionVote);
        // Before v2.0 the nVersionVote was used to specify the serialization version
        if (nVersionVote < PROTOCOL_V2_0)
        {
            nVersion = nVersionVote;
            if (fRead) const_cast<CVote*>(this)->nVersionVote = 0;
        }

        READWRITE(vCustodianVote);
        READWRITE(vParkRateVote);
        if (nVersion >= PROTOCOL_V0_5)
            READWRITE(vMotion);
        else
            ReadWriteSingleMotion(s, nType, nVersion, ser_action);
        if (nVersion >= PROTOCOL_V2_0)
            READWRITE(mapFeeVote);
        else if (fRead)
            const_cast<CVote*>(this)->mapFeeVote.clear();

        if (nVersion >= PROTOCOL_V4_0)
        {
            READWRITE(vReputationVote);
            READWRITE(signerReward);
            READWRITE(vAssetVote);
        }
        else if (fRead)
        {
            const_cast<CVote*>(this)->vReputationVote.clear();
            const_cast<CVote*>(this)->signerReward.SetNull();
            const_cast<CVote*>(this)->vAssetVote.clear();
        }
    )

    CScript ToScript(int nProtocolVersion) const;

    bool IsValid(int nProtocolVersion) const;
    bool IsValidInBlock(int nProtocolVersion) const;

    inline bool operator==(const CVote& other) const
    {
        return (nVersionVote == other.nVersionVote &&
                vCustodianVote == other.vCustodianVote &&
                vParkRateVote == other.vParkRateVote &&
                vMotion == other.vMotion &&
                mapFeeVote == other.mapFeeVote &&
                vReputationVote == other.vReputationVote &&
                signerReward == other.signerReward &&
                vAssetVote == other.vAssetVote);
    }
    inline bool operator!=(const CVote& other) const
    {
        return !(*this == other);
    }
};

class CUserVote : public CVote
{
public:
    int nVersion;

    CUserVote() :
        nVersion(PROTOCOL_VERSION)
    {
    }

    CUserVote(const CVote& vote, int nVersion = PROTOCOL_VERSION) :
        CVote(vote),
        nVersion(nVersion)
    {
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        nSerSize += SerReadWrite(s, *(CVote*)this, nType, nVersion, ser_action);
    )

    void Upgrade()
    {
        nVersion = PROTOCOL_VERSION;
        nVersionVote = PROTOCOL_VERSION;
    }

    CVote GenerateBlockVote(int nProtocolVersion) const;

    inline bool operator==(const CUserVote& other) const
    {
        return (nVersion == other.nVersion &&
                CVote::operator==(other));
    }
    inline bool operator!=(const CUserVote& other) const
    {
        return !(*this == other);
    }
};

bool IsVote(const CScript& scriptPubKey);
bool ExtractVote(const CScript& scriptPubKey, CVote& voteRet, int nProtocolVersion);
bool ExtractVote(const CBlock& block, CVote& voteRet, int nProtocolVersion);
bool ExtractVotes(const CBlock &block, const CBlockIndex *pindexprev, unsigned int nCount, std::vector<CVote> &vVoteResult);

bool IsParkRateResult(const CScript& scriptPubKey);
bool ExtractParkRateResult(const CScript& scriptPubKey, CParkRateVote& parkRateResultRet);
bool ExtractParkRateResults(const CBlock& block, std::vector<CParkRateVote>& vParkRateResultRet);

bool CalculateParkRateVote(const std::vector<CVote>& vVote, std::vector<CParkRateVote>& results);
bool LimitParkRateChangeV0_5(std::vector<CParkRateVote>& results, const std::map<unsigned char, std::vector<const CParkRateVote*> >& mapPreviousVotes);
bool LimitParkRateChangeV2_0(std::vector<CParkRateVote>& results, const std::map<unsigned char, const CParkRateVote*>& mapPreviousVotedRate);
bool CalculateParkRateResults(const CVote &vote, const CBlockIndex* pindexprev, int nProtocolVersion, std::vector<CParkRateVote>& vParkRateResult);
int64 GetPremium(int64 nValue, int64 nDuration, unsigned char cUnit, const std::vector<CParkRateVote>& vParkRateResult);

bool GenerateCurrencyCoinBases(const std::vector<CVote> vVote, const std::map<CBitcoinAddress, CBlockIndex*>& mapAlreadyElected, std::vector<CTransaction>& vCurrencyCoinBaseRet);

int GetProtocolForNextBlock(const CBlockIndex* pPrevIndex);
bool IsProtocolActiveForNextBlock(const CBlockIndex* pPrevIndex, int nSwitchTime, int nProtocolVersion, int nRequired, int nToCheck);

bool CalculateVotedFees(CBlockIndex* pindex);

bool CalculateReputationDestinationResult(const CBlockIndex* pindex, std::map<CTxDestination, int64>& mapReputation);
bool CalculateReputationResult(const CBlockIndex* pindex, std::map<CBitcoinAddress, int64>& mapReputation);

bool CalculateSignerReward(const CBlockIndex* pindex, CTxDestination& addressRet, int64& nRewardRet);
bool CalculateSignerRewardRecipient(const std::map<CTxDestination, int64>& mapReputation, int nCount, const std::map<CTxDestination, int>& mapPastReward, bool& fFoundRet, CTxDestination& addressRecipientRet);
bool GetPastSignerRewards(const CBlockIndex* pindex, std::map<CTxDestination, int>& mapPastRewardRet);

int CalculateSignerRewardVoteCounts(const CBlockIndex* pindex, std::map<int16_t, int>& mapCountRet, std::map<int32_t, int>& mapAmountRet);
bool CalculateSignerRewardVoteResult(CBlockIndex* pindex);

bool ExtractAssetVoteResult(const CBlockIndex *pindex, std::vector<CAsset> &vAssets);
bool CalculateVotedAssets(CBlockIndex* pindex);

bool MustUpgradeProtocol(const CBlockIndex* pPrevIndex, int nProtocolVersion);

#endif
