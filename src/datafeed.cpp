// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <curl/curl.h>
#include <cstdlib>
#include <cstring>
#include "datafeed.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include "vote.h"
#include "main.h"
#include "bitcoinrpc.h"
#include "wallet.h"

using namespace std;
using namespace json_spirit;

string strDataFeedError = "";

class DataFeedRequest
{
private:
    CURL *curl;
    string result;
    string error;
    string url;

    size_t ReceiveCallback(char *contents, size_t size, size_t nmemb)
    {
        if (size == 0 || nmemb == 0)
            return 0;

        size_t written = 0;

        try
        {
            size_t realsize = size * nmemb;

            int nMaxSize = GetArg("maxdatafeedsize", 10 * 1024);
            if (result.size() + realsize > nMaxSize)
                throw runtime_error((boost::format("Data feed size exceeds limit (%1% bytes)") % nMaxSize).str());

            result.append(contents, realsize);
            written = realsize;
        }
        catch (exception &e)
        {
            error = string(e.what());
        }

        return written;
    }

    static size_t WriteMemoryCallback(char *contents, size_t size, size_t nmemb, void *userp)
    {
        DataFeedRequest *request = (DataFeedRequest*)userp;
        return request->ReceiveCallback(contents, size, nmemb);
    }

public:
    DataFeedRequest(const string& sURL)
        : url(sURL)
    {
        curl = curl_easy_init();
        if (curl == NULL)
            throw runtime_error("Failed to initialize curl");

        curl_easy_setopt(curl, CURLOPT_URL, sURL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

        if (fUseProxy)
        {
            curl_easy_setopt(curl, CURLOPT_PROXY, addrProxy.ToStringIP().c_str());
            curl_easy_setopt(curl, CURLOPT_PROXYPORT, addrProxy.GetPort());
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
        }
    }

    ~DataFeedRequest()
    {
        if (curl)
        {
            curl_easy_cleanup(curl);
            curl = NULL;
        }
    }

    void Perform()
    {
        result.clear();
        error.clear();

        CURLcode res;
        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
        {
            long http_code = 0;
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK)
                throw runtime_error("Unable to get data feed response code");

            if (http_code != 200)
                throw runtime_error((boost::format("Data feed failed: Response code %ld") % http_code).str());

            printf("Received %lu bytes from data feed %s:\n%s\n", (long)result.size(), url.c_str(), result.c_str());
        }
        else
        {
            if (error == "")
                error = (boost::format("Data feed failed: %s") % curl_easy_strerror(res)).str();

            throw runtime_error(error);
        }
    }

    string GetResult() const
    {
        return result;
    }
};

void VerifyDataFeedSignature(const string& strMessage, const string& strSign, const string& strAddress)
{
    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw runtime_error("Invalid data feed address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw runtime_error("Data feed address does not refer to key");

    bool fInvalid = true;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw runtime_error("Malformed base64 encoding of data feed signature");

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig))
        throw runtime_error("Unable to set key from data feed signature");

    if (key.GetPubKey().GetID() != keyID)
        throw runtime_error("Data feed signature check failed");
}

bool GetVoteFromDataFeed(const CDataFeed& dataFeed, CVote& voteRet)
{
    bool result = false;

    try
    {
        DataFeedRequest request(dataFeed.sURL);
        request.Perform();

        if (dataFeed.sSignatureAddress != "" && dataFeed.sSignatureURL != "")
        {
            DataFeedRequest signatureRequest(dataFeed.sSignatureURL);
            signatureRequest.Perform();

            VerifyDataFeedSignature(request.GetResult(), signatureRequest.GetResult(), dataFeed.sSignatureAddress);
        }

        Value valReply;
        if (!read_string(request.GetResult(), valReply))
            throw runtime_error("Data feed returned invalid JSON data");

        CVote vote = ParseVote(valReply.get_obj());
        if (!vote.IsValid(pindexBest->nProtocolVersion))
            throw runtime_error("Data feed vote is invalid");

        voteRet = vote;
        result = true;
    }
    catch (exception &e)
    {
        printf("GetVoteFromDataFeed error: %s\n", e.what());
        throw;
    }

    return result;
}

CVote ParseVote(const Object& objVote)
{
    CVote vote;

    BOOST_FOREACH(const Pair& voteAttribute, objVote)
    {
        if (voteAttribute.name_ == "versionvote")
        {
            // Ignored
        }
        else if (voteAttribute.name_ == "motions")
        {
            BOOST_FOREACH(const Value& motionVoteObject, voteAttribute.value_.get_array())
                vote.vMotion.push_back(uint160(motionVoteObject.get_str()));
        }
        else if (voteAttribute.name_ == "custodians")
        {
            BOOST_FOREACH(const Value& custodianVoteObject, voteAttribute.value_.get_array())
            {
                CCustodianVote custodianVote;
                BOOST_FOREACH(const Pair& custodianVoteAttribute, custodianVoteObject.get_obj())
                {
                    if (custodianVoteAttribute.name_ == "address")
                    {
                        CBitcoinAddress address(custodianVoteAttribute.value_.get_str());
                        if (!address.IsValid())
                            throw runtime_error("Invalid address");

                        custodianVote.SetAddress(address);

                        // We only check if the unit is valid, not if a particular unit is valid
                        // for the current protocol version. For this call CVote::IsValid
                        if (!IsValidUnit(custodianVote.cUnit))
                            throw runtime_error("Invalid custodian unit");
                    }
                    else if (custodianVoteAttribute.name_ == "amount")
                        custodianVote.nAmount = AmountFromValue(custodianVoteAttribute.value_);
                    else
                        throw runtime_error("Invalid custodian vote object");
                }
                vote.vCustodianVote.push_back(custodianVote);
            }
        }
        else if (voteAttribute.name_ == "parkrates")
        {
            BOOST_FOREACH(const Value& parkRateVoteObject, voteAttribute.value_.get_array())
            {
                CParkRateVote parkRateVote;
                BOOST_FOREACH(const Pair& parkRateVoteAttribute, parkRateVoteObject.get_obj())
                {
                    if (parkRateVoteAttribute.name_ == "unit")
                    {
                        parkRateVote.cUnit = parkRateVoteAttribute.value_.get_str().c_str()[0];
                        if (!IsValidCurrency(parkRateVote.cUnit))
                            throw runtime_error("Invalid park rate unit");
                    }
                    else if (parkRateVoteAttribute.name_ == "rates")
                    {
                        BOOST_FOREACH(const Value& parkRateObject, parkRateVoteAttribute.value_.get_array())
                        {
                            CParkRate parkRate;
                            BOOST_FOREACH(const Pair& parkRateAttribute, parkRateObject.get_obj())
                            {
                                if (parkRateAttribute.name_ == "blocks")
                                {
                                   int blocks = parkRateAttribute.value_.get_int();
                                   double compactDuration = log2(blocks);
                                   double integerPart;
                                   if (modf(compactDuration, &integerPart) != 0.0)
                                       throw runtime_error("Park duration is not a power of 2");
                                   if (!CompactDurationRange(compactDuration))
                                       throw runtime_error("Park duration out of range");
                                   parkRate.nCompactDuration = compactDuration;
                                }
                                else if (parkRateAttribute.name_ == "rate")
                                {
                                    double dAmount = parkRateAttribute.value_.get_real();
                                    if (dAmount < 0.0 || dAmount > MAX_MONEY)
                                        throw runtime_error("Invalid park rate amount");
                                    parkRate.nRate = roundint64(dAmount * COIN_PARK_RATE);
                                    if (!MoneyRange(parkRate.nRate))
                                        throw runtime_error("Invalid park rate amount");
                                }
                                else
                                    throw runtime_error("Invalid park rate object");
                            }
                            parkRateVote.vParkRate.push_back(parkRate);
                        }
                    }
                    else
                        throw runtime_error("Invalid custodian vote object");
                }
                vote.vParkRateVote.push_back(parkRateVote);
            }
        }
        else if (voteAttribute.name_ == "fees")
        {
            map<unsigned char, uint32_t> feeVotes;
            BOOST_FOREACH(const Pair& feeVoteAttribute, voteAttribute.value_.get_obj())
            {
                string unitString = feeVoteAttribute.name_;
                if (unitString.size() != 1)
                    throw runtime_error("Invalid fee unit");

                unsigned char cUnit = unitString[0];
                if (!IsValidUnit(cUnit))
                    throw runtime_error("Invalid fee unit");

                double feeAsCoin = feeVoteAttribute.value_.get_real();
                if (feeAsCoin < 0 || feeAsCoin * COIN > numeric_limits<uint32_t>::max())
                    throw runtime_error("Invalid fee amount");
                uint32_t feeAsSatoshi = feeAsCoin * COIN;

                feeVotes[cUnit] = feeAsSatoshi;
            }
            vote.mapFeeVote = feeVotes;
        }
        else if (voteAttribute.name_ == "reputations")
        {
            BOOST_FOREACH(const Value& reputationVoteObject, voteAttribute.value_.get_array())
            {
                CReputationVote reputationVote;
                BOOST_FOREACH(const Pair& reputationVoteAttribute, reputationVoteObject.get_obj())
                {
                    if (reputationVoteAttribute.name_ == "address")
                    {
                        CBitcoinAddress address(reputationVoteAttribute.value_.get_str());
                        if (!address.IsValid())
                            throw runtime_error("Invalid address");
                        if (address.GetUnit() != '8')
                            throw runtime_error("Invalid address unit");

                        reputationVote.SetAddress(address);
                    }
                    else if (reputationVoteAttribute.name_ == "weight")
                    {
                        int nWeight = reputationVoteAttribute.value_.get_int();
                        if (nWeight < -127 || nWeight > 127)
                            throw runtime_error("Invalid reputation weight");
                        reputationVote.nWeight = nWeight;
                    }
                    else
                        throw runtime_error("Invalid reputation vote object");
                }
                vote.vReputationVote.push_back(reputationVote);
            }
        }
        else if (voteAttribute.name_ == "signerreward")
        {
            BOOST_FOREACH(const Pair& signerRewardAttribute, voteAttribute.value_.get_obj())
            {
                if (signerRewardAttribute.name_ == "count")
                {
                    if (signerRewardAttribute.value_.type() == null_type || signerRewardAttribute.value_.get_int() < 0)
                        vote.signerReward.nCount = -1;
                    else
                    {
                        int nCount = signerRewardAttribute.value_.get_int();
                        if (nCount < numeric_limits<int16_t>::min() || nCount > numeric_limits<int16_t>::max())
                            throw runtime_error("Invalid signer reward count");
                        vote.signerReward.nCount = (int16_t) nCount;
                    }
                }
                else if (signerRewardAttribute.name_ == "amount")
                {
                    if (signerRewardAttribute.value_.type() == null_type || signerRewardAttribute.value_.get_real() < 0)
                        vote.signerReward.nAmount = -1;
                    else
                    {
                        double amountAsSatoshi = signerRewardAttribute.value_.get_real() * COIN;
                        if (amountAsSatoshi < numeric_limits<int32_t>::min() || amountAsSatoshi > numeric_limits<int32_t>::max())
                            throw runtime_error("Invalid amount");
                        vote.signerReward.nAmount = (int32_t)amountAsSatoshi;
                    }
                }
                else
                    throw runtime_error("Invalid signer reward vote object");
            }
        }
        else if (voteAttribute.name_ == "assets")
        {
            BOOST_FOREACH(const Value& assetVoteObject, voteAttribute.value_.get_array())
            {
                CAssetVote assetVote;
                double maxTrade = 0.0;
                double minTrade = 0.0;
                BOOST_FOREACH(const Pair& assetVoteAttribute, assetVoteObject.get_obj())
                {
                    if (assetVoteAttribute.name_ == "assetid")
                    {
                        uint32_t nAssetId = EncodeAssetId(assetVoteAttribute.value_.get_str());

                        if (!IsValidAssetId(nAssetId))
                            throw runtime_error("Invalid asset id");

                        assetVote.nAssetId = nAssetId;
                    }
                    else if (assetVoteAttribute.name_ == "confirmations")
                    {
                        int nConfirmations = assetVoteAttribute.value_.get_int();
                        if (nConfirmations <= 0 || nConfirmations > MAX_CONFIRMATIONS)
                            throw runtime_error("Invalid confirmations");
                        assetVote.nNumberOfConfirmations = (uint16_t)nConfirmations;
                    }
                    else if (assetVoteAttribute.name_ == "reqsigners")
                    {
                        int nRequiredSigners = assetVoteAttribute.value_.get_int();
                        if (nRequiredSigners <= 0 || nRequiredSigners > MAX_SIGNERS)
                            throw runtime_error("Invalid required signers quantity");
                        if (assetVote.nTotalDepositSigners > 0 && nRequiredSigners > assetVote.nTotalDepositSigners )
                            throw runtime_error("Required signers cannot be more than the total signers");
                        assetVote.nRequiredDepositSigners = (uint8_t)nRequiredSigners;
                    }
                    else if (assetVoteAttribute.name_ == "totalsigners")
                    {
                        int nTotalSigners = assetVoteAttribute.value_.get_int();
                        if (nTotalSigners <= 0 || nTotalSigners > MAX_SIGNERS)
                            throw runtime_error("Invalid total signers quantity");
                        if (assetVote.nRequiredDepositSigners > 0 && nTotalSigners < assetVote.nRequiredDepositSigners )
                            throw runtime_error("Total signers cannot be less than the required signers");
                        assetVote.nTotalDepositSigners = (uint8_t)nTotalSigners;
                    }
                    else if (assetVoteAttribute.name_ == "maxtrade")
                    {
                        maxTrade = assetVoteAttribute.value_.get_real();
                        if (maxTrade < 0)
                            throw runtime_error("Invalid max trade value");
                        // Set later when we find out the exponent
                    }
                    else if (assetVoteAttribute.name_ == "mintrade")
                    {
                        minTrade = assetVoteAttribute.value_.get_real();
                        if (minTrade < 0)
                            throw runtime_error("Invalid minimum trade value");
                        // Set later when we find out the exponent
                    }
                    else if (assetVoteAttribute.name_ == "unitexponent")
                    {
                        int nUnitExponent = assetVoteAttribute.value_.get_int();
                        if (nUnitExponent <= 0 || nUnitExponent > MAX_TRADABLE_UNIT_EXPONENT)
                            throw runtime_error("Invalid unit exponent value");
                        assetVote.nUnitExponent = nUnitExponent;
                    }
                    else
                        throw runtime_error("Invalid asset vote object");
                }
                // Set the max/min trades as we know the unit exponent
                assetVote.nMaxTradeExpParam = ExponentialParameter(maxTrade * pow(10, assetVote.nUnitExponent));
                assetVote.nMinTradeExpParam = ExponentialParameter(minTrade * pow(10, assetVote.nUnitExponent));
                vote.vAssetVote.push_back(assetVote);
            }
        }
        else
            throw runtime_error("Invalid vote object");
    }

    return vote;
}

void UpdateFromDataFeed()
{
    CWallet* pwallet = GetWallet('8');
    CDataFeed dataFeed;
    {
        LOCK(pwallet->cs_wallet);
        dataFeed = pwallet->GetDataFeed();
    }

    if (dataFeed.sURL == "")
        return;

    printf("Updating from data feed %s\n", dataFeed.sURL.c_str());

    CVote feedVote;
    if (GetVoteFromDataFeed(dataFeed, feedVote))
    {
        LOCK(pwallet->cs_wallet);
        CVote newVote = pwallet->vote;
        BOOST_FOREACH(const string& sPart, dataFeed.vParts)
        {
            if (sPart == "custodians")
                newVote.vCustodianVote = feedVote.vCustodianVote;
            else if (sPart == "parkrates")
                newVote.vParkRateVote = feedVote.vParkRateVote;
            else if (sPart == "motions")
                newVote.vMotion = feedVote.vMotion;
            else if (sPart == "fees")
                newVote.mapFeeVote = feedVote.mapFeeVote;
            else if (sPart == "reputations")
                newVote.vReputationVote = feedVote.vReputationVote;
            else if (sPart == "signerreward")
                newVote.signerReward = feedVote.signerReward;
            else if (sPart == "assets")
                newVote.vAssetVote = feedVote.vAssetVote;
            else
                throw runtime_error("Invalid part");
        }
        {
            LOCK(pwallet->cs_wallet);
            pwallet->SetVote(newVote);
        }
        printf("Vote updated from data feed\n");
    }
}

void ThreadUpdateFromDataFeed(void* parg)
{
    printf("ThreadUpdateFromDataFeed started\n");

    int delay = GetArg("-datafeeddelay", 60);
    int64 lastUpdate = GetTime();

    while (true)
    {
#ifdef TESTING
        Sleep(200);
#else
        Sleep(1000);
#endif

        if (fShutdown)
            break;

        int64 now = GetTime();

        if (now >= lastUpdate + delay)
        {
            strDataFeedError = "";
            try
            {
                UpdateFromDataFeed();
            }
            catch (exception &e)
            {
                printf("UpdateFromDataFeed failed: %s\n", e.what());
                strDataFeedError = string(e.what());
            }

            lastUpdate = now;
        }
    }

    printf("ThreadUpdateFromDataFeed exited\n");
}

void StartUpdateFromDataFeed()
{
    if (!CreateThread(ThreadUpdateFromDataFeed, NULL))
        printf("Error; CreateThread(ThreadUpdateFromDataFeed) failed\n");
}
