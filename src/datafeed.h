// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DATA_FEED_H
#define DATA_FEED_H

#include <string>
#include "json/json_spirit_value.h"
#include "serialize.h"

class CVote;

class CDataFeed
{
public:
    int nVersion;
    std::string sURL;
    std::string sSignatureURL;
    std::string sSignatureAddress;
    std::vector<std::string> vParts;

    CDataFeed()
    {
        vParts.push_back("custodians");
        vParts.push_back("parkrates");
        vParts.push_back("motions");
    }

    CDataFeed(const std::string& sURL, const std::string& sSignatureURL, const std::string& sSignatureAddress, const std::vector<std::string>& vParts) :
        sURL(sURL),
        sSignatureURL(sSignatureURL),
        sSignatureAddress(sSignatureAddress),
        vParts(vParts)
    {
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(sURL);
        READWRITE(sSignatureURL);
        READWRITE(sSignatureAddress);
        READWRITE(vParts);
    )

};

CVote ParseVote(const json_spirit::Object& objVote);
void StartUpdateFromDataFeed();
void UpdateFromDataFeed();

extern std::string strDataFeedError;

#endif
