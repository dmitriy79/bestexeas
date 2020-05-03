// Copyright (c) 2015 The B&C Exchange developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <map>
#include <string>

#include "coinmetadata.h"
#include "util.h"

#define COIN_SYMBOL_INDEX (0)
#define COIN_NAME_INDEX (1)
#define COIN_UNIT_EXP_INDEX (2)

using std::string;
using boost::assign::map_list_of;

// TODO maybe move in a config file
coin_metadata_map const COIN_METADATA = map_list_of
        (EncodeAssetId("BKS"),   coin_metadata("BKS",   "Blockshares",  4))
        (EncodeAssetId("BKC"),   coin_metadata("BKC",   "Blockcredits", 4))
        (EncodeAssetId("BTC"),   coin_metadata("BTC",   "Bitcoin",      8))
        (EncodeAssetId("LTC"),   coin_metadata("LTC",   "Litecoin",     8))
        (EncodeAssetId("DOGE"),  coin_metadata("DOGE",  "Dogecoin",     8))
        (EncodeAssetId("RDD"),   coin_metadata("RDD",   "Reddcoin",     8))
        (EncodeAssetId("DASH"),  coin_metadata("DASH",  "Dash",         8))
        (EncodeAssetId("PPC"),   coin_metadata("PPC",   "Peercoin",     6))
        (EncodeAssetId("NMC"),   coin_metadata("NMC",   "Namecoin",     8))
        (EncodeAssetId("FTC"),   coin_metadata("FTC",   "Feathercoin",  8))
        (EncodeAssetId("XCP"),   coin_metadata("XCP",   "Counterparty", 8))
        (EncodeAssetId("BLK"),   coin_metadata("BLK",   "Blackcoin",    8))
        (EncodeAssetId("NSR"),   coin_metadata("NSR",   "NuShares",     8))
        (EncodeAssetId("NBT"),   coin_metadata("NBT",   "NuBits",       4))
        (EncodeAssetId("MZC"),   coin_metadata("MZC",   "Mazacoin",     8))
        (EncodeAssetId("VIA"),   coin_metadata("VIA",   "Viacoin",      8))
        (EncodeAssetId("RBY"),   coin_metadata("RBY",   "Rubycoin",     8))
        (EncodeAssetId("GRS"),   coin_metadata("GRS",   "Groestlcoin",  8))
        (EncodeAssetId("DGC"),   coin_metadata("DGC",   "Digitalcoin",  8))
        (EncodeAssetId("CCN"),   coin_metadata("CCN",   "Cannacoin",    8))
        (EncodeAssetId("DGB"),   coin_metadata("DGB",   "DigiByte",     8))
        (EncodeAssetId("NVC"),   coin_metadata("NVC",   "Novacoin",     6))
        (EncodeAssetId("MONA"),  coin_metadata("MONA",  "Monacoin",     8))
        (EncodeAssetId("CLAM"),  coin_metadata("CLAM",  "Clams",        6))
        (EncodeAssetId("XPM"),   coin_metadata("XPM",   "Primecoin",    6))
        (EncodeAssetId("NEOS"),  coin_metadata("NEOS",  "Neoscoin",     8))
        (EncodeAssetId("JBS"),   coin_metadata("JBS",   "Jumbucks",     8))
        (EncodeAssetId("PND"),   coin_metadata("PND",   "Pandacoin",    8))
        (EncodeAssetId("VTC"),   coin_metadata("VTC",   "Vertcoin",     8))
        (EncodeAssetId("NXT"),   coin_metadata("NXT",   "NXT",          8))
        (EncodeAssetId("BURST"), coin_metadata("BURST", "Burst",        8))
        (EncodeAssetId("VPN"),   coin_metadata("VPN",   "Vpncoin",      8))
        (EncodeAssetId("CDN"),   coin_metadata("CDN",   "Canada eCoin", 8))
        (EncodeAssetId("SDC"),   coin_metadata("SDC",   "ShadowCash",   8))
        ;

std::string GetAssetSymbol(uint32_t gid)
{
    coin_metadata_map::const_iterator itMetadata = COIN_METADATA.find(gid);
    if (itMetadata == COIN_METADATA.end())
        return "";
    coin_metadata metadata = itMetadata->second;
    return metadata.get<COIN_SYMBOL_INDEX>();
}

std::string GetAssetName(uint32_t gid)
{
    coin_metadata_map::const_iterator itMetadata = COIN_METADATA.find(gid);
    if (itMetadata == COIN_METADATA.end())
        return "";
    coin_metadata metadata = itMetadata->second;
    return metadata.get<COIN_NAME_INDEX>();
}

unsigned char GetAssetUnitExponent(uint32_t gid)
{
    coin_metadata_map::const_iterator itMetadata = COIN_METADATA.find(gid);
    if (itMetadata == COIN_METADATA.end())
        return 0;
    coin_metadata metadata = itMetadata->second;
    return metadata.get<COIN_UNIT_EXP_INDEX>();
}
