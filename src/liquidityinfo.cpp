// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/algorithm/string.hpp>

#include "liquidityinfo.h"
#include "main.h"
#include "ui_interface.h"

#define LIQUIDITY_INFO_MAX_HEIGHT 260000 // Liquidity info from custodians who were elected more than LIQUIDITY_INFO_MAX_HEIGHT blocks ago are ignored

#ifdef TESTING
#define MAX_LIQUIDITY_INFO_PER_CUSTODIAN 5
#else
#define MAX_LIQUIDITY_INFO_PER_CUSTODIAN 100
#endif

using namespace std;

map<const CLiquiditySource, CLiquidityInfo> mapLiquidityInfo;
CCriticalSection cs_mapLiquidityInfo;
int64 nLastLiquidityUpdate = 0;

void RemoveExtraLiquidityInfo(const CBitcoinAddress& custodianAddress)
{
    LOCK(cs_mapLiquidityInfo);

    int nCount = 0;
    map<int64, map<const CLiquiditySource, CLiquidityInfo>::iterator> mapLiquidityInfoByTime;

    map<const CLiquiditySource, CLiquidityInfo>::iterator it = mapLiquidityInfo.begin();
    while (it != mapLiquidityInfo.end())
    {
        const CLiquidityInfo& info = it->second;
        const CBitcoinAddress& infoAddress = info.GetCustodianAddress();

        if (infoAddress == custodianAddress)
        {
            nCount++;
            mapLiquidityInfoByTime[info.nTime] = it;
        }
        it++;
    }

    if (nCount && nCount > MAX_LIQUIDITY_INFO_PER_CUSTODIAN)
    {
        const map<const CLiquiditySource, CLiquidityInfo>::iterator& oldestit = mapLiquidityInfoByTime.begin()->second;
        const CLiquidityInfo& oldestInfo = oldestit->second;
        printf("Custodian %s has too many liquidity info. Removing the oldest one: %s\n", custodianAddress.ToString().c_str(), oldestInfo.ToString().c_str());
        mapLiquidityInfo.erase(oldestit);
    }
}

bool CLiquidityInfo::ProcessLiquidityInfo()
{
    if (!IsValidCurrency(cCustodianUnit))
        return false;

    CBitcoinAddress address(GetCustodianAddress());

    {
        LOCK(cs_main);

        if (pindexBest->nProtocolVersion >= PROTOCOL_V2_0 && nVersion > pindexBest->nProtocolVersion)
            return false;

        map<CBitcoinAddress, CBlockIndex*> mapElectedCustodian = pindexBest->GetElectedCustodians();
        map<CBitcoinAddress, CBlockIndex*>::iterator it;
        it = mapElectedCustodian.find(address);
        if (it == mapElectedCustodian.end())
            return false;

        CBlockIndex *pindex = it->second;
        if (!pindexBest || pindexBest->nHeight - pindex->nHeight > LIQUIDITY_INFO_MAX_HEIGHT)
            return false;
    }

    if (!CheckSignature())
        return false;

    if (!IsValid())
        return false;

    CLiquiditySource source(address, sIdentifier);

    {
        LOCK(cs_mapLiquidityInfo);

        map<const CLiquiditySource, CLiquidityInfo>::iterator it;
        it = mapLiquidityInfo.find(source);
        if (it != mapLiquidityInfo.end())
        {
            if (it->second.nTime >= nTime)
                return false;
        }

        mapLiquidityInfo[source] = *this;
        nLastLiquidityUpdate = GetAdjustedTime();
    }

    printf("accepted liquidity info from %s\n", address.ToString().c_str());

    RemoveExtraLiquidityInfo(address);

    MainFrameRepaint();
    return true;
}

string CUnsignedLiquidityInfo::GetTier() const
{
    vector<string> items;
    boost::split(items, sIdentifier, boost::is_any_of(":"));

    if (items.size() > 0)
    {
        const string& tierString = items[0];
        if (!tierString.empty() && tierString.find_first_not_of("0123456789") == std::string::npos)
            return tierString;
    }
    return "";
}

void RemoveExpiredLiquidityInfo(int nCurrentHeight)
{
    bool fAnyRemoved = false;
    {
        LOCK2(cs_main, cs_mapLiquidityInfo);

        map<CBitcoinAddress, CBlockIndex*> mapElectedCustodian = pindexBest->GetElectedCustodians();
        map<const CLiquiditySource, CLiquidityInfo>::iterator it;
        it = mapLiquidityInfo.begin();
        while (it != mapLiquidityInfo.end())
        {
            const CBitcoinAddress& address = it->first.custodianAddress;
            CBlockIndex *pindex = NULL;

            map<CBitcoinAddress, CBlockIndex*>::const_iterator electedIt = mapElectedCustodian.find(address);
            if (electedIt != mapElectedCustodian.end())
                pindex = electedIt->second;

            if (!pindex || nCurrentHeight - pindex->nHeight > LIQUIDITY_INFO_MAX_HEIGHT)
            {
                mapLiquidityInfo.erase(it++);
                fAnyRemoved = true;
                nLastLiquidityUpdate = GetAdjustedTime();
            }
            else
                it++;
        }
    }

    if (fAnyRemoved)
        MainFrameRepaint();
}
