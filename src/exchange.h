// Copyright (c) 2015 The B&C Exchange developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EXCHANGE_H
#define EXCHANGE_H

#include "serialize.h"
#include "util.h"

#define MAX_TRADABLE_UNIT_EXPONENT (18)

class CAsset
{
public:
    uint32_t nAssetId;
    uint16_t nNumberOfConfirmations;
    uint8_t nRequiredDepositSigners;
    uint8_t nTotalDepositSigners;
    uint8_t nMaxTradeExpParam;
    uint8_t nMinTradeExpParam;
    uint8_t nUnitExponent;

    CAsset() :
        nAssetId(0),
        nNumberOfConfirmations(0),
        nRequiredDepositSigners(0),
        nTotalDepositSigners(0),
        nMaxTradeExpParam(0),
        nMinTradeExpParam(0),
        nUnitExponent(0)
    {
    }

    void SetNull()
    {
        nAssetId = 0;
        nNumberOfConfirmations = 0;
        nRequiredDepositSigners = 0;
        nTotalDepositSigners = 0;
        nMaxTradeExpParam = 0;
        nMinTradeExpParam = 0;
        nUnitExponent = 0;
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

    inline int64 GetMaxTrade() const
    {
        return nMaxTradeExpParam <= EXP_SERIES_MAX_PARAM ? pnExponentialSeries[nMaxTradeExpParam] : 0;
    }

    inline int64 GetMinTrade() const
    {
        return nMinTradeExpParam <= EXP_SERIES_MAX_PARAM ? pnExponentialSeries[nMinTradeExpParam] : 0;
    }

    inline bool operator==(const CAsset& other) const
    {
        return (nAssetId == other.nAssetId &&
                nNumberOfConfirmations == other.nNumberOfConfirmations &&
                nRequiredDepositSigners == other.nRequiredDepositSigners &&
                nTotalDepositSigners == other.nTotalDepositSigners &&
                nMaxTradeExpParam == other.nMaxTradeExpParam &&
                nMinTradeExpParam == other.nMinTradeExpParam &&
                nUnitExponent == other.nUnitExponent);
    }
    inline bool operator!=(const CAsset& other) const
    {
        return !(*this == other);
    }
};

#endif //EXCHANGE_H
