#include <boost/test/unit_test.hpp>
#include <algorithm>

#include "main.h"
#include "vote.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(vote_tests)

BOOST_AUTO_TEST_CASE(reload_vote_from_script_tests)
{
    CVote vote;

    CCustodianVote custodianVote;
    CBitcoinAddress custodianAddress(CKeyID(123465), 'C');
    custodianVote.SetAddress(custodianAddress);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);

    CCustodianVote custodianVote2;
    CBitcoinAddress custodianAddress2(CKeyID(555555555), 'C');
    custodianVote2.SetAddress(custodianAddress2);
    custodianVote2.nAmount = 5.5 * COIN;
    vote.vCustodianVote.push_back(custodianVote2);

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'C';
    parkRateVote.vParkRate.push_back(CParkRate(13, 3));
    parkRateVote.vParkRate.push_back(CParkRate(14, 6));
    parkRateVote.vParkRate.push_back(CParkRate(15, 13));
    vote.vParkRateVote.push_back(parkRateVote);

    vote.vMotion.push_back(uint160(123456));
    vote.vMotion.push_back(uint160(3333));

    CScript script = vote.ToScript(PROTOCOL_VERSION);

    CVote voteResult;
    BOOST_CHECK(ExtractVote(script, voteResult, PROTOCOL_VERSION));

#undef CHECK_VOTE_EQUAL
#define CHECK_VOTE_EQUAL(value) BOOST_CHECK(voteResult.value == vote.value);
    CHECK_VOTE_EQUAL(vCustodianVote.size());
    for (int i=0; i<vote.vCustodianVote.size(); i++)
    {
        CHECK_VOTE_EQUAL(vCustodianVote[i].cUnit);
        CHECK_VOTE_EQUAL(vCustodianVote[i].hashAddress);
        CHECK_VOTE_EQUAL(vCustodianVote[i].GetAddress().ToString());
        CHECK_VOTE_EQUAL(vCustodianVote[i].nAmount);
    }
    BOOST_CHECK_EQUAL(custodianAddress.ToString(), vote.vCustodianVote[0].GetAddress().ToString());
    BOOST_CHECK_EQUAL(custodianAddress2.ToString(), vote.vCustodianVote[1].GetAddress().ToString());

    CHECK_VOTE_EQUAL(vParkRateVote.size());
    for (int i=0; i<vote.vParkRateVote.size(); i++)
    {
        CHECK_VOTE_EQUAL(vParkRateVote[i].cUnit);
        CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate.size());
        for (int j=0; j<vote.vParkRateVote[i].vParkRate.size(); j++)
        {
            CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate[j].nCompactDuration);
            CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate[j].nRate);
        }
    }

    CHECK_VOTE_EQUAL(vMotion.size());
    CHECK_VOTE_EQUAL(vMotion[0]);
    CHECK_VOTE_EQUAL(vMotion[1]);
#undef CHECK_VOTE_EQUAL
}

BOOST_AUTO_TEST_CASE(reload_park_rates_from_script_tests)
{
    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'C';
    parkRateVote.vParkRate.push_back(CParkRate(13, 3));
    parkRateVote.vParkRate.push_back(CParkRate(14, 6));
    parkRateVote.vParkRate.push_back(CParkRate(15, 13));

    CScript script = parkRateVote.ToParkRateResultScript();
    BOOST_CHECK(IsParkRateResult(script));

    CParkRateVote parkRateVoteResult;
    BOOST_CHECK(ExtractParkRateResult(script, parkRateVoteResult));

#undef CHECK_PARK_RATE_EQUAL
#define CHECK_PARK_RATE_EQUAL(value) BOOST_CHECK(parkRateVoteResult.value == parkRateVote.value);
    CHECK_PARK_RATE_EQUAL(cUnit);
    CHECK_PARK_RATE_EQUAL(vParkRate.size());
    for (int i=0; i<parkRateVote.vParkRate.size(); i++)
    {
        CHECK_PARK_RATE_EQUAL(vParkRate[i].nCompactDuration);
        CHECK_PARK_RATE_EQUAL(vParkRate[i].nRate);
    }
#undef CHECK_PARK_RATE_EQUAL
}

template< class T >
static void shuffle(vector<T> v)
{
    random_shuffle(v.begin(), v.end());
}

BOOST_AUTO_TEST_CASE(rate_calculation_from_votes)
{
    vector<CVote> vVote;
    vector<CParkRateVote> results;

    // Result of empty vote is empty
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(0, results.size());

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'C';
    parkRateVote.vParkRate.push_back(CParkRate( 8, 100));

    CVote vote;
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 1000;

    vVote.push_back(vote);

    // Single vote: same result as vote
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // New vote with same weight and bigger rate
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(8, 200));
    vote.vParkRateVote.push_back(parkRateVote);
    vVote.push_back(vote);

    // Two votes of same weight, the median is the first one
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // Vote 2 has a little more weight
    vVote[1].nCoinAgeDestroyed = 1001;

    // Each coin age has a vote. So the median is the second vote rate.
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(200, results[0].vParkRate[0].nRate);

    // New vote with small weight and rate between the 2 first
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(8, 160));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 3;
    vVote.push_back(vote);

    // The median is the middle rate
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(160, results[0].vParkRate[0].nRate);

    // New vote with another duration
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(9, 300));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 100;
    vVote.push_back(vote);

    // It votes for 0 on duration 8, so the result is back to 100
    // On duration 9 everybody else vote for 0, so the median is 0, so there's no result
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // New vote with multiple durations unordered
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(13, 500));
    parkRateVote.vParkRate.push_back(CParkRate(9, 400));
    parkRateVote.vParkRate.push_back(CParkRate(8, 200));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 2050;
    vVote.push_back(vote);

    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    // On duration 8:
    // Vote weights: 0: 100, 100: 1000, 160: 3, 200: 3051
    // So median is 200
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(200, results[0].vParkRate[0].nRate);
    // On duration 9:
    // Vote weights: 0: 2004, 300: 100, 400: 2050
    // So median is 300
    BOOST_CHECK_EQUAL(  9, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(300, results[0].vParkRate[1].nRate);
    // On duration 13: only last vote is positive and it has not the majority, so median is 0
    BOOST_CHECK_EQUAL(  2, results[0].vParkRate.size());

    // Shuffle all the votes
    srand(1234);
    BOOST_FOREACH(const CVote& vote, vVote)
    {
        BOOST_FOREACH(const CParkRateVote& parkRateVote, vote.vParkRateVote)
            shuffle(parkRateVote.vParkRate);
        shuffle(vote.vParkRateVote);
    }
    shuffle(vVote);

    // The result should not be changed
    vector<CParkRateVote> newResults;
    BOOST_CHECK(CalculateParkRateVote(vVote, newResults));
    BOOST_CHECK(results == newResults);
}

BOOST_AUTO_TEST_CASE(rate_limitation_v05)
{
    vector<CParkRateVote> baseResults, results;
    map<unsigned char, vector<const CParkRateVote*> > previousRates;

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'C';
    parkRateVote.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN)); // 1 year
    baseResults.push_back(parkRateVote);

    int64 maxIncreaseDuration15 = pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration18 = pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration19 = pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    // Without previous rates, the previous rates are all considered 0 so the rate increase is limited
    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With an empty previous rate (meaning the rates are all 0), the rate increase is limited
    CParkRateVote previousRate;
    previousRate.cUnit = 'C';
    previousRates['C'].push_back(&previousRate);

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With some previous rates
    previousRate.vParkRate.push_back(CParkRate(10, 100 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(15, 3 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 950 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(3 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);

    // With multiple previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 2 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 150 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(2 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(150 * COIN_PARK_RATE / COIN + maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // Decrease is not limited
    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(10, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(15, 10 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 0 * COIN_PARK_RATE / COIN)); // 1 year
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(10 * COIN_PARK_RATE / COIN, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1 * COIN_PARK_RATE / COIN, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(0 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);
}

BOOST_AUTO_TEST_CASE(rate_limitation_v06)
{
    vector<CParkRateVote> baseResults, results;
    map<unsigned char, const CParkRateVote*> mapPreviousRate;

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'C';
    parkRateVote.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN)); // 1 year
    baseResults.push_back(parkRateVote);

    int64 maxIncreaseDuration15 = 0.002 * pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration18 = 0.002 * pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration19 = 0.002 * pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    // Without previous rates, the previous rates are all considered 0 so the rate increase is limited
    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With an empty previous rate (meaning all rates were 0), the rate increase is limited
    CParkRateVote previousRate;
    previousRate.cUnit = 'C';
    mapPreviousRate['C'] = &previousRate;

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With some previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 3 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 999.9 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(3 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);

    // With multiple previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 2 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 150 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(2 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(150 * COIN_PARK_RATE / COIN + maxIncreaseDuration19, results[0].vParkRate[2].nRate);


    // Check decrease limits
    int64 maxDecreaseDuration15 = 0.004 * pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxDecreaseDuration18 = 0.004 * pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxDecreaseDuration19 = 0.004 * pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(15, 10 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18 + 5)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 0 * COIN_PARK_RATE / COIN)); // 1 year
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18 + 5, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration19, results[0].vParkRate[2].nRate);

    // Decrease limit when no park rates are voted
    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(18, 0)); // 6 months
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration19, results[0].vParkRate[2].nRate);

}

BOOST_AUTO_TEST_CASE(vote_validity_tests)
{
    CVote vote;
    CParkRateVote parkRateVote;

    // An empty vote is valid
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on share is invalid
    parkRateVote.cUnit = '8';
    vote.vParkRateVote.push_back(parkRateVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on unknown unit is invalid
    vote.vParkRateVote[0].cUnit = 'A';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on nubits is valid
    vote.vParkRateVote[0].cUnit = 'C';
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two park rate vote on nubits is invalid
    parkRateVote.cUnit = 'C';
    vote.vParkRateVote.push_back(parkRateVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // Park rate with duration and 0 rate is valid
    vote.vParkRateVote.erase(vote.vParkRateVote.end());
    CParkRate parkRate;
    parkRate.nCompactDuration = 0;
    parkRate.nRate = 0;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two valid park rates
    parkRate.nCompactDuration = 4;
    parkRate.nRate = 100;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two times the same duration is invalid
    parkRate.nCompactDuration = 4;
    parkRate.nRate = 200;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));
    vote.vParkRateVote[0].vParkRate.pop_back();

    // A valid custodian vote
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'C';
    custodianVote.hashAddress = uint160(1);
    custodianVote.nAmount = 8 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Another unit is invalid
    vote.vCustodianVote[0].cUnit = 'A';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));
    // BlockShares grants are invalid pre v2.0
    vote.vCustodianVote[0].cUnit = '8';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_V0_5));
    // But valid after v2.0
    BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
    vote.vCustodianVote[0].cUnit = 'C';

    // Voting for the same custodian and amount twice is invalid
    vote.vCustodianVote.push_back(custodianVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // If the amount is different it is valid
    vote.vCustodianVote[0].nAmount++;
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));
    vote.vCustodianVote[0].nAmount--;

    // If the address is different it is valid
    vote.vCustodianVote[0].hashAddress++;
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));
    vote.vCustodianVote[0].hashAddress--;

    {
        // Reputation vote only valid from 4.0
        CVote vote;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));
        vote.vReputationVote.push_back(CReputationVote(CBitcoinAddress("8VZRy4CAWKVC2HBWFE9YppLki5FJJhfrkY"), 1));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));
    }

    {
        // Signer reward vote only valid from 4.0
        CVote vote;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));

        vote.signerReward.nCount = -1;
        vote.signerReward.nAmount = -1;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));

        vote.signerReward.nCount = 0;
        vote.signerReward.nAmount = 0;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));

        vote.signerReward.nCount = 1;
        vote.signerReward.nAmount = 0;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));

        vote.signerReward.nCount = 0;
        vote.signerReward.nAmount = 1;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));

        vote.signerReward.nCount = 10;
        vote.signerReward.nAmount = 21;
        BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
        BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V2_0));
        BOOST_CHECK(vote.IsValid(PROTOCOL_V4_0));
        BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0));
    }
}

void printVotes(vector<CVote> vVote)
{
    BOOST_FOREACH(const CVote& vote, vVote)
        BOOST_FOREACH(const CCustodianVote& custodianVote, vote.vCustodianVote)
            printf("addr=%d, amount=%d, weight=%d, unit=%c\n", custodianVote.hashAddress.Get64(), custodianVote.nAmount, vote.nCoinAgeDestroyed, custodianVote.cUnit);
    printf("\n");
}

BOOST_AUTO_TEST_CASE(create_currency_coin_bases)
{
    vector<CVote> vVote;
    std::map<CBitcoinAddress, CBlockIndex*> mapAlreadyElected;

    // Zero vote results in no new currency
    vector<CTransaction> vCurrencyCoinBase;
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a vote without custodian vote
    CVote vote;
    vote.nCoinAgeDestroyed = 1000;
    vVote.push_back(vote);

    // Still no currency created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a custodian vote with the same coin age
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'C';
    custodianVote.hashAddress = uint160(1);
    custodianVote.nAmount = 8 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    vVote.push_back(vote);

    // Still no currency created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // The last vote has a little more weight
    vVote.back().nCoinAgeDestroyed++;

    // Still no currency created because this vote does not have the majority of blocks (we have 2 votes)
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a 3rd vote for the same custodian
    vVote.back().nCoinAgeDestroyed--;
    vote.nCoinAgeDestroyed = 1;
    vVote.push_back(vote);

    // This custodian should win and currency should be created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    CTransaction tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('C', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    CTxDestination address;
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // This custodian has already been elected
    mapAlreadyElected[CBitcoinAddress(address, 'C')] = new CBlockIndex;

    // He should not receive any new currency
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a vote for another custodian to the existing votes
    custodianVote.hashAddress = uint160(2);
    custodianVote.nAmount = 5 * COIN;
    vVote[0].vCustodianVote.push_back(custodianVote);
    vVote[1].vCustodianVote.push_back(custodianVote);

    // And clear the already elected
    mapAlreadyElected.clear();

    // Both should receive new currency
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('C', tx.cUnit);
    BOOST_CHECK_EQUAL(2, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[1].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[1].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(2).ToString(), boost::get<CKeyID>(address).ToString());

    // But if they have the same address
    uint160 hashAddress = vVote[1].vCustodianVote.front().hashAddress;
    vVote[0].vCustodianVote.back().hashAddress = hashAddress;
    vVote[1].vCustodianVote.back().hashAddress = hashAddress;
    vVote[2].vCustodianVote.back().hashAddress = hashAddress;

//    printVotes(vVote);

    // Only the amount with the highest coin age is granted
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());

    tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('C', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // BlockShare grants are valid now
    vVote[0].vCustodianVote.back().cUnit = '8';
    vVote[1].vCustodianVote.back().cUnit = '8';
    vVote[2].vCustodianVote.back().cUnit = '8';
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[0];
    tx.nTime = 2000000000; // set a time that is after V06 switch time
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK_EQUAL('8', tx.cUnit);
    BOOST_CHECK_GE(2, tx.vout.size()); // BKS currency coin base has an empty first output
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // Check if both BKS and BKC grants can happen
    vVote[2].vCustodianVote.back().cUnit = 'C';

    // Should BKS and BKC
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(2, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[1];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('C', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());
    tx = vCurrencyCoinBase[0];
    tx.nTime = 2000000000; // set a time that is after V06 switch time
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK_EQUAL('8', tx.cUnit);
    BOOST_CHECK_GE(2, tx.vout.size()); // BKS currency coin base has an empty first output
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // Unknown units should fail but we can only check if the CVote is valid or not
    vVote[1].vCustodianVote.back().cUnit = '?';
    BOOST_CHECK(!vVote[1].IsValid(PROTOCOL_VERSION));
}

BOOST_AUTO_TEST_CASE(premium_calculation)
{
    vector<CParkRateVote> vParkRateResult;
    CParkRateVote parkRateResult;
    parkRateResult.cUnit = 'C';
    parkRateResult.vParkRate.push_back(CParkRate( 2,  5 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate( 5, 50 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate( 3, 10 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate(10,  1 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(12,  2 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(13,  5 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(15, 50 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(16, 40 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(17, 50 * COIN_PARK_RATE));
    vParkRateResult.push_back(parkRateResult);

    // Below minimum rate
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 0, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 1, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 3, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(10 * COIN, 3, 'C', vParkRateResult));

    // Above maximum rate
    BOOST_CHECK_EQUAL( 0, GetPremium(   1 * COIN, (1<<17)+1, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(1000 * COIN, (1<<17)+1, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(   1 * COIN,   1000000, 'C', vParkRateResult));

    // Exact durations
    BOOST_CHECK_EQUAL( 5, GetPremium(1   * COIN,  4, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(10, GetPremium(2   * COIN,  4, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(0.1 * COIN,  4, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(10, GetPremium(1   * COIN,  8, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(99, GetPremium(9.9 * COIN,  8, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(50, GetPremium(1   * COIN, 32, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 1 * COIN, GetPremium(1 * COIN,  1024, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 2 * COIN, GetPremium(1 * COIN,  4096, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 5 * COIN, GetPremium(1 * COIN,  8192, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(50 * COIN, GetPremium(1 * COIN, 1<<15, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(40 * COIN, GetPremium(1 * COIN, 1<<16, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(50 * COIN, GetPremium(1 * COIN, 1<<17, 'C', vParkRateResult));

    // Intermediate durations
    BOOST_CHECK_EQUAL( 6, GetPremium(1   * COIN,  5, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 9, GetPremium(1.5 * COIN,  5, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(25, GetPremium(4   * COIN,  5, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 8, GetPremium(1   * COIN,  7, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL( 8, GetPremium(1   * COIN,  7, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(21, GetPremium(1   * COIN, 15, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL(38, GetPremium(1   * COIN, 25, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL((int64)(3.39453125 * COIN), GetPremium(1 * COIN, 6000, 'C', vParkRateResult));
    BOOST_CHECK_EQUAL((int64)(49.69482421875 * COIN), GetPremium(1 * COIN, (1<<15) + 1000, 'C', vParkRateResult));
}

BOOST_AUTO_TEST_CASE(effective_park_rates_delayed_after_protocol_v2_0)
{
    CBlockIndex *pindexBase = NULL;

    int64 nValue = 1000 * COIN;
    unsigned char cUnit = 'C';
    int nCompactDuration = 5;
    int nDuration = (1 << nCompactDuration);

    // Create 100 previous blocks
    CBlockIndex *pindex = NULL;
    for (int i = 0; i < 100; i++)
    {
        if (i == 0)
        {
            pindex = new CBlockIndex;
            pindexBase = pindex;
        }
        else
        {
            pindex->pprev = new CBlockIndex;
            pindex->pprev->pnext = pindex;
            pindex = pindex->pprev;
        }

        vector<CParkRateVote> vParkRateResult;
        CParkRateVote parkRateResult;
        parkRateResult.cUnit = cUnit;
        parkRateResult.vParkRate.push_back(CParkRate(nCompactDuration, (i+1) * COIN_PARK_RATE / COIN));
        vParkRateResult.push_back(parkRateResult);
        pindex->vParkRateResult = vParkRateResult;
    }

    pindexBase->nProtocolVersion = PROTOCOL_V0_5;
    int64 expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexBase->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetPremium(nValue, nDuration, cUnit));
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetNextPremium(nValue, nDuration, cUnit));

    pindexBase->nProtocolVersion = PROTOCOL_V2_0;
    CBlockIndex* pindexEffective = pindexBase;
    for (int i = 0; i < 60; i++)
        pindexEffective = pindexEffective->pprev;
    expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexEffective->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetPremium(nValue, nDuration, cUnit));

    pindexEffective = pindexEffective->pnext;
    expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexEffective->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetNextPremium(nValue, nDuration, cUnit));
}

BOOST_AUTO_TEST_CASE(protocol_voting)
{
    /* Test disabled on BCExchange because protocol starts at 2_0. Enable again when there's a 3_0 protocol switch to test.
    int PROTOCOL_SWITCH_TIME = 100;
    int PROTOCOL_VOTES_REQ = 70;
    int PROTOCOL_VOTES_TOTAL = 80;
    int PROTOCOL_VERSION = PROTOCOL_V2_0;
    CBlockIndex* pIndexFirst = NULL;
    CBlockIndex* pIndexBest = NULL;
    CBlockIndex* pCurrent = NULL;
    // Create test indexes
    for(int i = 0; i < 300; i++)
    {
        CBlockIndex* pPrevIndex = pIndexBest;
        pIndexBest = new CBlockIndex();
        pIndexBest->nHeight = i;
        pIndexBest->nTime = i + 1;
        pIndexBest->pprev = pPrevIndex;
        if (pPrevIndex)
            pPrevIndex->pnext = pIndexBest;
        if (pIndexFirst == NULL)
            pIndexFirst = pIndexBest;
    }
    // Vote passes on switch time as majority achieved
    for(pCurrent = pIndexFirst; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = PROTOCOL_VERSION;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        // After the switch time, check that we have the new protocol
        if (pCurrent->pprev != NULL && pCurrent->pprev->nTime >= PROTOCOL_SWITCH_TIME)
        {
            BOOST_CHECK_EQUAL(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        }
        else // Before the switch time we are still using the old protocol
        {
            BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        }
    }
    // Reset
    for(pCurrent = pIndexFirst; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = 0;
        pCurrent->nProtocolVersion = 0;
    }
    // Vote will pass after switch time as majority is not achieved immediately
    BOOST_CHECK_LT(pIndexBest->nProtocolVersion, PROTOCOL_VERSION);
    pCurrent = pIndexFirst;
    // First 100 blocks don't vote for new protocol
    for(int i = 0; i < 99; i++)
    {
        pCurrent->vote.nVersionVote = 0;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        pCurrent = pCurrent->pnext;
    }
    // The rest of the blocks vote for the new protocol so on time 170 we switch protocol
    for(; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = PROTOCOL_VERSION;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        if (pCurrent->nTime >= 170)
            BOOST_CHECK_EQUAL(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        else
            BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
    }
    BOOST_CHECK_EQUAL(pIndexBest->nProtocolVersion, PROTOCOL_VERSION);
    // Free mem
    while(pIndexBest != NULL)
    {
        CBlockIndex* pIndex = pIndexBest;
        pIndexBest = pIndexBest->pprev;
        delete pIndex;
    }
    */
}

struct CProtocolSwitchTestChain
{
    CBlockIndex* pindexBest;
    CBlockIndex* pindex;

    CProtocolSwitchTestChain(int nStartTime)
    {
        pindexBest = new CBlockIndex;
        pindex = pindexBest;
        pindex->nProtocolVersion = PROTOCOL_V2_0;
        pindex->nTime = 1441116000;
    }

    ~CProtocolSwitchTestChain()
    {
        CBlockIndex* pi = pindexBest;
        pindexBest = NULL;
        pindex = NULL;
        while (pi)
        {
            CBlockIndex* pd = pi;
            pi = pd->pprev;
            pd->pprev = NULL;
            if (pi)
                pi->pnext = NULL;
            delete pd;
        }
    }

    CBlockIndex* AddChild()
    {
        pindex->pnext = new CBlockIndex;
        pindex->pnext->pprev = pindex;
        pindex = pindex->pnext;
        return pindex;
    }

    void AddVotes(int nCount, int nVersionVote)
    {
        for (int i = 0; i < nCount; i++)
        {
            CBlockIndex *pindex = AddChild();
            pindex->nTime = pindex->pprev->nTime + 60;
            pindex->nProtocolVersion = GetProtocolForNextBlock(pindex->pprev);;
            pindex->vote.nVersionVote = nVersionVote;
        }
    }

    CBlockIndex* AddTimeBlock(int nTime)
    {
        CBlockIndex *pindex = AddChild();
        pindex->nTime = nTime;
        pindex->nProtocolVersion = GetProtocolForNextBlock(pindex->pprev);;
        return pindex;
    }
};

BOOST_AUTO_TEST_CASE(protocol_v4_switch)
{
    const int week = 7 * 24 * 3600;
    const int nStartTime = 1441116000; // 2015-09-01 14:00:00 UTC

    {
        // With only 2.0 votes, no switch
        CProtocolSwitchTestChain chain(nStartTime);
        chain.AddVotes(4000, PROTOCOL_V2_0);
        chain.AddTimeBlock(chain.pindex->nTime + 3 * week);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);
    }

    {
        // With almost a switch
        CProtocolSwitchTestChain chain(nStartTime);
        chain.AddVotes(2000, PROTOCOL_V2_0);
        chain.AddVotes(1000, PROTOCOL_V4_0);
        chain.AddVotes( 201, PROTOCOL_V2_0);
        chain.AddVotes(1799, PROTOCOL_V4_0);
        chain.AddVotes(1000, PROTOCOL_V2_0);
        for (int i = 0; i < 5; i++)
        {
            chain.AddTimeBlock(chain.pindex->nTime + 3 * week + i * 3600);
            BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);
        }
    }

    {
        // With a switch
        CProtocolSwitchTestChain chain(nStartTime);
        chain.AddVotes(2000, PROTOCOL_V2_0);
        chain.AddVotes(1800, PROTOCOL_V4_0);

        // The last block is the one that passes 90%
        unsigned int nVotePassTime = chain.pindex->nTime;
        BOOST_CHECK_EQUAL(nStartTime + 3800 * 60, nVotePassTime); // 2015-09-04 05:20:00 UTC

        // The blocks after do not change the protocol
        chain.AddVotes(2000, PROTOCOL_V2_0);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);

        // Exactly 2 weeks after, the protocol is still 2.0
        chain.AddTimeBlock(nVotePassTime + 2 * week);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);

        // And also on the next blocks
        chain.AddVotes(100, PROTOCOL_V2_0);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);

        // The expected switch time
        unsigned int nSwitchTime = 1442584800; // 2015-09-18 14:00:00 UTC (14 days after at 14:00)

        // On 2015-09-04 at 13:59:59 UTC the protocol is still 2.0
        chain.AddTimeBlock(nSwitchTime - 1);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);

        // The first block after 14:00:00 UTC triggers the switch, but its effective only on the next block
        chain.AddTimeBlock(nSwitchTime);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);
        BOOST_CHECK_EQUAL(PROTOCOL_V4_0, GetProtocolForNextBlock(chain.pindex));

        // The first block after is 4.0
        chain.AddTimeBlock(nSwitchTime + 1);
        BOOST_CHECK_EQUAL(PROTOCOL_V4_0, chain.pindex->nProtocolVersion);

        // All all the others after
        chain.AddVotes(2000, PROTOCOL_V2_0);
        BOOST_CHECK_EQUAL(PROTOCOL_V4_0, chain.pindex->nProtocolVersion);
    }

    {
        // With a switch before 14:00 because a whole day was skipped (may happen during testing or on testnet)
        CProtocolSwitchTestChain chain(nStartTime);
        chain.AddVotes(2000, PROTOCOL_V2_0);
        chain.AddVotes(1800, PROTOCOL_V4_0);

        // The last block is the one that passes 90%
        unsigned int nVotePassTime = chain.pindex->nTime;
        BOOST_CHECK_EQUAL(nStartTime + 3800 * 60, nVotePassTime); // 2015-09-04 05:20:00 UTC

        // The expected switch time
        unsigned int nSwitchTime = 1442584800; // 2015-09-18 14:00:00 UTC (14 days after at 14:00)

        // The first block after 2015-09-04 14:00:00 UTC is the day after
        chain.AddTimeBlock(nSwitchTime + 16 * 3600);
        BOOST_CHECK_EQUAL(PROTOCOL_V2_0, chain.pindex->nProtocolVersion);
        BOOST_CHECK_EQUAL(PROTOCOL_V4_0, GetProtocolForNextBlock(chain.pindex));

        // The first block after is 4.0
        chain.AddTimeBlock(nSwitchTime + 16 * 3600 + 60);
        BOOST_CHECK_EQUAL(PROTOCOL_V4_0, chain.pindex->nProtocolVersion);
    }
}

BOOST_AUTO_TEST_CASE(vote_v50000_unserialization)
{
    // Serialized with v0.5.2 vote code:
    /* {
    CVote vote;
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'C';
    custodianVote.hashAddress = uint160(123465);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    vote.vMotion.push_back(uint160(123456));
    vote.vMotion.push_back(uint160(3333));
    printf("%s\n", vote.ToScript().ToString().c_str());
    printf("%s\n", HexStr(vote.ToScript()).c_str());
    } */
    vector<unsigned char> oldVoteString = ParseHex("6a514c4d50c3000001420049e201000000000000000000000000000000000040420f0000000000000240e2010000000000000000000000000000000000050d000000000000000000000000000000000000");

    CScript oldVoteScript(oldVoteString.begin(), oldVoteString.end());

    CVote vote;
    BOOST_CHECK(ExtractVote(oldVoteScript, vote, PROTOCOL_VERSION));

    BOOST_CHECK_EQUAL(0, vote.mapFeeVote.size());
}

BOOST_AUTO_TEST_CASE(reputation_weight_unserialization)
{
    // Vote in block 552252 which includes a downvote
    vector<unsigned char> voteString = ParseHex("6a514c52404b4c00000000000300c2a32ea884de2476cdb63740e1e280cfda893ea2010030aa11671d18bf275ae99e2d67b8678754cc26470100fd155810e3536c79d648158b7b7388a579606e8eff03006400000000");

    CScript voteScript(voteString.begin(), voteString.end());

    CVote vote;
    BOOST_CHECK(ExtractVote(voteScript, vote, PROTOCOL_VERSION));

    BOOST_CHECK_EQUAL(3, vote.vReputationVote.size());
    BOOST_CHECK_EQUAL("8Yq1JGmKAm4qyitx6atpUz1QQqqFm5Ki19", vote.vReputationVote[0].GetAddress().ToString());
    BOOST_CHECK_EQUAL(1, vote.vReputationVote[0].nWeight);
    BOOST_CHECK_EQUAL("8KXAthBdtPWokQRXB93FzPTKvBH5WXdTxV", vote.vReputationVote[1].GetAddress().ToString());
    BOOST_CHECK_EQUAL(1, vote.vReputationVote[1].nWeight);
    BOOST_CHECK_EQUAL("8eA3FVjGUE2KFDJy6qSQxZb3MZdoqLf3ZL", vote.vReputationVote[2].GetAddress().ToString());
    BOOST_CHECK_EQUAL(-1, vote.vReputationVote[2].nWeight);
}

BOOST_AUTO_TEST_CASE(fee_calculation)
{
    BOOST_CHECK_EQUAL(   0, CalculateFee(   0,    1));
    BOOST_CHECK_EQUAL(   1, CalculateFee(   1,    1));
    BOOST_CHECK_EQUAL( 123, CalculateFee(1000,  123));
    BOOST_CHECK_EQUAL(  11, CalculateFee( 101,  100));
    BOOST_CHECK_EQUAL( 236, CalculateFee(2121,  111));
}

vector<CBlockIndex*> feeVoteIndexes;
int lastFeeVoteIndex = -1;

void AddFeeVoteBlocks(int nCount, int64 nFeeVote8, int64 nFeeVoteC)
{
    for (int i = 0; i < nCount; i++)
    {
        CBlockIndex* pindex = new CBlockIndex();
        if (lastFeeVoteIndex >= 0)
        {
            pindex->pprev = feeVoteIndexes[lastFeeVoteIndex];
            pindex->pprev->pnext = pindex;
        }
        feeVoteIndexes.push_back(pindex);
        lastFeeVoteIndex++;
        pindex->nHeight = lastFeeVoteIndex;
        if (nFeeVote8 != -1)
            pindex->vote.mapFeeVote['8'] = nFeeVote8;
        if (nFeeVoteC != -1)
            pindex->vote.mapFeeVote['C'] = nFeeVoteC;
        BOOST_CHECK(CalculateVotedFees(pindex));
    }
}

void CheckFeeOnTransactions(CBlockIndex *pindex, int64 nExpected8Fee, int64 nExpectedCFee)
{
    {
        CTransaction tx;
        tx.cUnit = '8';
        for (int i = 0; i < 3000; i+= 125)
            BOOST_CHECK_EQUAL(CalculateFee(i, nExpected8Fee), tx.GetMinFee(pindex, i));
        BOOST_CHECK_EQUAL(tx.GetMinFee(pindex, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)), tx.GetMinFee(pindex));
    }
    {
        CTransaction tx;
        tx.cUnit = 'C';
        for (int i = 0; i < 3000; i+= 194)
            BOOST_CHECK_EQUAL(CalculateFee(i, nExpectedCFee), tx.GetMinFee(pindex, i));
        BOOST_CHECK_EQUAL(tx.GetMinFee(pindex, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)), tx.GetMinFee(pindex));
    }
}

#define CHECK_VOTED_MIN_FEE(nIndex, nExpected8Fee, nExpectedCFee) { \
    BOOST_CHECK_EQUAL(nExpected8Fee, feeVoteIndexes[nIndex]->GetVotedMinFee('8')); \
    BOOST_CHECK_EQUAL(nExpectedCFee, feeVoteIndexes[nIndex]->GetVotedMinFee('C')); \
}

#define CHECK_EFFECTIVE_MIN_FEE(nIndex, nExpected8Fee, nExpectedCFee) { \
    BOOST_CHECK_EQUAL(nExpected8Fee, feeVoteIndexes[nIndex]->GetMinFee('8')); \
    BOOST_CHECK_EQUAL(nExpectedCFee, feeVoteIndexes[nIndex]->GetMinFee('C')); \
    CheckFeeOnTransactions(feeVoteIndexes[nIndex], nExpected8Fee, nExpectedCFee); \
}

#define CHECK_SAFE_MIN_FEE(nIndex, nExpected8Fee, nExpectedCFee) { \
    BOOST_CHECK_EQUAL(nExpected8Fee, feeVoteIndexes[nIndex]->GetSafeMinFee('8')); \
    BOOST_CHECK_EQUAL(nExpectedCFee, feeVoteIndexes[nIndex]->GetSafeMinFee('C')); \
}

void ResetFeeVoteBlocks()
{
    BOOST_FOREACH(CBlockIndex* pindex, feeVoteIndexes)
        delete pindex;
    feeVoteIndexes.clear();
    lastFeeVoteIndex = -1;
}

BOOST_AUTO_TEST_CASE(fee_vote_calculation)
{
    // 3000 blocks not including any fee vote
    AddFeeVoteBlocks(3000,     -1, -1);
    // 3500 blocks voting for a new BKS fee and no vote for the BKC fee
    AddFeeVoteBlocks( 500, 2*COIN, -1);
    AddFeeVoteBlocks(1000, 3*COIN, -1);
    AddFeeVoteBlocks(2000, 1*CENT, -1);

    // The first blocks have the default fee
    for (int i = 0; i < 4000; i++)
    {
        CHECK_VOTED_MIN_FEE    (i, CENT, COIN);
        CHECK_EFFECTIVE_MIN_FEE(i, CENT, COIN);
        CHECK_SAFE_MIN_FEE     (i, CENT, COIN);
    }

    // The votes started at block 3000, so at block 4000 there are 1001 votes so the voted fee changes
    CHECK_VOTED_MIN_FEE    (4000, 2*COIN, COIN);
    // But not the other ones
    CHECK_EFFECTIVE_MIN_FEE(4000,   CENT, COIN);
    CHECK_SAFE_MIN_FEE     (4000,   CENT, COIN);

    // The effective fee doesn't change until 60 blocks have passed
    for (int i = 4000; i < 4060; i++)
        CHECK_EFFECTIVE_MIN_FEE(i, CENT, COIN);
    CHECK_EFFECTIVE_MIN_FEE(4060, 2*COIN, COIN);

    // The safe fee changes 10 blocks before the effective fee
    for (int i = 4000; i < 4050; i++)
        CHECK_SAFE_MIN_FEE(i, CENT, COIN);
    CHECK_SAFE_MIN_FEE(4050, 2*COIN, COIN);


    ResetFeeVoteBlocks();

    AddFeeVoteBlocks(1001, -1, 10*CENT);
    AddFeeVoteBlocks( 500, -1,  3*CENT);
    AddFeeVoteBlocks( 500, -1,  5*CENT);
    AddFeeVoteBlocks(1000, -1,  1*CENT);

    CHECK_VOTED_MIN_FEE(2000, 1*CENT, 5*CENT);
    CHECK_VOTED_MIN_FEE(2499, 1*CENT, 5*CENT);
    CHECK_VOTED_MIN_FEE(2500, 1*CENT, 3*CENT);

    CHECK_EFFECTIVE_MIN_FEE(2559, 1*CENT, 5*CENT);
    CHECK_EFFECTIVE_MIN_FEE(2560, 1*CENT, 3*CENT);

    CHECK_SAFE_MIN_FEE(2549, 1*CENT, 5*CENT);
    CHECK_SAFE_MIN_FEE(2550, 1*CENT, 5*CENT);
    CHECK_SAFE_MIN_FEE(2558, 1*CENT, 5*CENT); // the next block still has a 5 cents fee
    CHECK_SAFE_MIN_FEE(2559, 1*CENT, 3*CENT); // there's no more 5 cents blocks

    ResetFeeVoteBlocks();
}

BOOST_AUTO_TEST_CASE(reputation_vote_distribution)
{
    CUserVote vote;
    CReputationVote reputationVote;

    CBitcoinAddress address1("8VZRy4CAWKVC2HBWFE9YppLki5FJJhfrkY");
    reputationVote.SetAddress(address1);
    reputationVote.nWeight = 5;
    vote.vReputationVote.push_back(reputationVote);

    CBitcoinAddress address2("8FJryeHhudFYJQvPvJ3CgCMkc2oHNLtNhj");
    reputationVote.SetAddress(address2);
    reputationVote.nWeight = 10;
    vote.vReputationVote.push_back(reputationVote);

    CBitcoinAddress address3("9EQRQJaFeFNu5yiGrDEvmMjSLf2aiYuLTN");
    reputationVote.SetAddress(address3);
    reputationVote.nWeight = 1;
    vote.vReputationVote.push_back(reputationVote);

    CBitcoinAddress address4("8XHmT2idfoCPRs939XBrxiPbE3VLedg2cj");
    reputationVote.SetAddress(address4);
    reputationVote.nWeight = -5;
    vote.vReputationVote.push_back(reputationVote);

    CBitcoinAddress address5("8GnxVP1s9jJADMKNbTe5AFqb9mSy5KaArg");
    reputationVote.SetAddress(address5);
    reputationVote.nWeight = 0;
    vote.vReputationVote.push_back(reputationVote);

    int nTotalWeight = 5 + 10 + 1 + 5 + 0;

    map<CBitcoinAddress, int> mapAddressCount;
    for (int i = 0; i < 10000; i++)
    {
        CVote blockVote = vote.GenerateBlockVote(PROTOCOL_V4_0);
        BOOST_CHECK_EQUAL(3, blockVote.vReputationVote.size());
        BOOST_FOREACH(const CReputationVote& reputationVote, blockVote.vReputationVote)
        {
            CBitcoinAddress address = reputationVote.GetAddress();
            if (address == address4)
                BOOST_CHECK_EQUAL(-1, reputationVote.nWeight);
            else
                BOOST_CHECK_EQUAL( 1, reputationVote.nWeight);
            mapAddressCount[address]++;
        }
    }

    BOOST_CHECK_CLOSE((double)3 * 10000 *  5 / nTotalWeight, (double)mapAddressCount[address1], 10);
    BOOST_CHECK_CLOSE((double)3 * 10000 * 10 / nTotalWeight, (double)mapAddressCount[address2], 10);
    BOOST_CHECK_CLOSE((double)3 * 10000 *  1 / nTotalWeight, (double)mapAddressCount[address3], 10);
    BOOST_CHECK_CLOSE((double)3 * 10000 *  5 / nTotalWeight, (double)mapAddressCount[address4], 10);
    BOOST_CHECK_CLOSE((double)3 * 10000 *  0 / nTotalWeight, (double)mapAddressCount[address5], 10);
}

BOOST_AUTO_TEST_CASE(reputation_vote_result)
{
    CBlockIndex* pindexBest = new CBlockIndex;
    CBlockIndex* pindex = pindexBest;

    map<CBitcoinAddress, int64> mapReputation;

    CBitcoinAddress address1("8VZRy4CAWKVC2HBWFE9YppLki5FJJhfrkY");
    CBitcoinAddress address2("8FJryeHhudFYJQvPvJ3CgCMkc2oHNLtNhj");
    CBitcoinAddress address3("9EQRQJaFeFNu5yiGrDEvmMjSLf2aiYuLTN");
    CBitcoinAddress address4("8XHmT2idfoCPRs939XBrxiPbE3VLedg2cj");

    // No vote, no reputation
    BOOST_CHECK(CalculateReputationResult(pindexBest, mapReputation));
    BOOST_CHECK_EQUAL(0, mapReputation.size());

    // A single vote should add the corresponding reputations
    // We are in the last 5000 blocks so the score is multiplied by 4
    pindex->vote.vReputationVote.push_back(CReputationVote(address1, 1));
    BOOST_CHECK(pindex->vote.IsValidInBlock(PROTOCOL_V4_0));
    BOOST_CHECK(CalculateReputationResult(pindexBest, mapReputation));
    BOOST_CHECK_EQUAL(1, mapReputation.size());
    BOOST_CHECK_EQUAL(4, mapReputation[address1]);

    // Multiple reputation votes in the same block should be counted
    pindex->pprev = new CBlockIndex;
    pindex = pindex->pprev;
    pindex->vote.vReputationVote.push_back(CReputationVote(address1, 1));
    pindex->vote.vReputationVote.push_back(CReputationVote(address2, -1));
    pindex->vote.vReputationVote.push_back(CReputationVote(address1, 1));
    BOOST_CHECK(pindex->vote.IsValidInBlock(PROTOCOL_V4_0));
    BOOST_CHECK(CalculateReputationResult(pindexBest, mapReputation));
    BOOST_CHECK_EQUAL(2, mapReputation.size());
    BOOST_CHECK_EQUAL(3*4, mapReputation[address1]);
    BOOST_CHECK_EQUAL(-1*4, mapReputation[address2]);

    // The first 5,000 votes have a weight of 4
    // The next 10,000 have a weight of 2
    // The next 20,000 have a weight of 1
    // We already have 2 blocks. Let's add some more.
    for (int i = 0; i < 4998; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->vote.vReputationVote.push_back(CReputationVote(address1, -1));
        pindex->vote.vReputationVote.push_back(CReputationVote(address3, 1));
    }
    // Next 10,000, weight 2
    for (int i = 0; i < 5000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->vote.vReputationVote.push_back(CReputationVote(address2, 1));
        pindex->vote.vReputationVote.push_back(CReputationVote(address2, 1));
        pindex->vote.vReputationVote.push_back(CReputationVote(address2, 1));
    }
    for (int i = 0; i < 5000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->vote.vReputationVote.push_back(CReputationVote(address4, 1));
        pindex->vote.vReputationVote.push_back(CReputationVote(address3, -1));
    }
    // Next 20,000, weight 1
    for (int i = 0; i < 20000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->vote.vReputationVote.push_back(CReputationVote(address4, -1));
    }
    // Next 1,000, weight 0
    for (int i = 0; i < 1000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->vote.vReputationVote.push_back(CReputationVote(address4, 1));
    }
    // Now we have:
    // address1: 3 upvotes and 4,998 downvotes at weight 4
    // address2: 1 downvote at weight 4 and 15,000 upvotes at weight 2
    // address3: 4,998 upvotes at weight 4 and 5,000 downvotes at weight 2
    // address4: 5,000 upvotes at weight 2 and 20,000 downvotes at weight 1
    BOOST_CHECK(pindex->vote.IsValidInBlock(PROTOCOL_V4_0));
    BOOST_CHECK(CalculateReputationResult(pindexBest, mapReputation));
    BOOST_CHECK_EQUAL(4, mapReputation.size());
    BOOST_CHECK_EQUAL((3-4998)*4, mapReputation[address1]);
    BOOST_CHECK_EQUAL(-1*4+15000*2, mapReputation[address2]);
    BOOST_CHECK_EQUAL(4998*4-5000*2, mapReputation[address3]);
    BOOST_CHECK_EQUAL(5000*2-20000*1, mapReputation[address4]);
}

BOOST_AUTO_TEST_CASE(reputation_reward_calculation)
{
    map<CTxDestination, int64> mapReputation;
    map<CTxDestination, int> mapPastReward;

    CKeyID address1(0x111);
    CKeyID address2(0x222);
    CKeyID address3(0x333);

    int nCount = 10;

    // No reputation, no recipient
    mapReputation.clear();
    mapPastReward.clear();
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(false, fFound);
        BOOST_CHECK(boost::get<CNoDestination>(&address));
    }

    // With a single reputed signer and no past reward
    mapReputation.clear();
    mapReputation[address1] = 1;
    mapPastReward.clear();
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address1.GetHex());
    }

    // With multiple reputed signer and no past reward
    mapReputation.clear();
    mapReputation[address1] = 1;
    mapReputation[address2] = 2;
    mapReputation[address3] = 1;
    mapPastReward.clear();
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address2.GetHex());
    }

    // With past reward already given to the most reputed signer
    mapReputation.clear();
    mapReputation[address1] = 1;
    mapReputation[address2] = 3;
    mapReputation[address3] = 2;
    mapPastReward.clear();
    mapPastReward[address2] = 1000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }

    // With equal distance, the reward goes to the highest hash
    mapReputation.clear();
    mapReputation[address1] = 1;
    mapReputation[address2] = 2;
    mapReputation[address3] = 1;
    mapPastReward.clear();
    mapPastReward[address2] = 1000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }

    // With more data
    mapReputation.clear();
    mapReputation[address1] = 20 * 1234;
    mapReputation[address2] = 50 * 1234;
    mapReputation[address3] = 30 * 1234;
    mapPastReward.clear();
    mapPastReward[address1] = 0.199 * 2000;
    mapPastReward[address2] = 0.503 * 2000;
    mapPastReward[address3] = 0.298 * 2000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }

    // Negative reputations do not receive reward
    mapReputation.clear();
    mapReputation[address1] = -20 * 1234;
    mapReputation[address2] =  10 * 1234;
    mapReputation[address3] = -10 * 1234;
    mapPastReward.clear();
    mapPastReward[address1] = 0.199 * 2000;
    mapPastReward[address2] = 0.503 * 2000;
    mapPastReward[address3] = 0.298 * 2000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address2.GetHex());
    }

    // Null reputation do not get reward
    mapReputation.clear();
    mapReputation[address1] = 0;
    mapReputation[address2] = -1;
    mapReputation[address3] = 1;
    mapPastReward.clear();
    mapPastReward[address1] = 0;
    mapPastReward[address2] = 0;
    mapPastReward[address3] = 2000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, nCount, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }

    // Only nCount reputed signers are eligible for the reward
    mapReputation.clear();
    mapReputation[address1] = 20 * 1234;
    mapReputation[address2] = 50 * 1234;
    mapReputation[address3] = 30 * 1234;
    mapPastReward.clear();
    mapPastReward[address1] = 0.10 * 2000;
    mapPastReward[address2] = 0.55 * 2000;
    mapPastReward[address3] = 0.25 * 2000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, 2, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, 1, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address2.GetHex());
    }

    // In case of a tie, the one who received the least rewards wins. If it's sill a tie, the one with the highest hash wins
    mapReputation.clear();
    mapReputation[address1] = 20 * 1234;
    mapReputation[address2] = 20 * 1234;
    mapReputation[address3] = 20 * 1234;
    mapPastReward.clear();
    mapPastReward[address1] = 0.40 * 2000;
    mapPastReward[address2] = 0.40 * 2000;
    mapPastReward[address3] = 0.20 * 2000;
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, 3, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address3.GetHex());
    }
    {
        bool fFound;
        CTxDestination address;
        BOOST_CHECK(CalculateSignerRewardRecipient(mapReputation, 2, mapPastReward, fFound, address));
        BOOST_CHECK_EQUAL(true, fFound);
        BOOST_CHECK_EQUAL(boost::get<CKeyID>(address).GetHex(), address2.GetHex());
    }
}

BOOST_AUTO_TEST_CASE(get_past_rewarded_signers)
{
    CBlockIndex* pindexBest = new CBlockIndex;
    CBlockIndex* pindex = pindexBest;

    CBitcoinAddress address1("8VZRy4CAWKVC2HBWFE9YppLki5FJJhfrkY");
    CBitcoinAddress address2("8FJryeHhudFYJQvPvJ3CgCMkc2oHNLtNhj");
    CBitcoinAddress address3("9EQRQJaFeFNu5yiGrDEvmMjSLf2aiYuLTN");
    CBitcoinAddress address4("8XHmT2idfoCPRs939XBrxiPbE3VLedg2cj");

    for (int i = 0; i < 1000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->rewardedSigner = address1.Get();
    }

    for (int i = 0; i < 100; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->rewardedSigner = CNoDestination();
    }

    for (int i = 0; i < 1000; i++)
    {
        pindex->pprev = new CBlockIndex;
        pindex = pindex->pprev;
        pindex->rewardedSigner = address3.Get();
    }

    std::map<CTxDestination, int> mapPastReward;
    BOOST_CHECK(GetPastSignerRewards(pindexBest, mapPastReward));
    BOOST_CHECK_EQUAL(   2, mapPastReward.size());
    BOOST_CHECK_EQUAL(1000, mapPastReward[address1.Get()]);
    BOOST_CHECK_EQUAL( 900, mapPastReward[address3.Get()]);
}

class CSignerRewardChain
{
public:
    CBlockIndex* pindexBest;
    CBlockIndex* pindex;

    CSignerRewardChain()
    {
        pindexBest = new CBlockIndex;
        pindexBest->nProtocolVersion = PROTOCOL_V4_0;
        pindex = pindexBest;
    }

    ~CSignerRewardChain()
    {
        CBlockIndex* pi = pindexBest;
        pindexBest = NULL;
        pindex = NULL;
        while (pi)
        {
            CBlockIndex* pd = pi;
            pi = pd->pprev;
            pd->pprev = NULL;
            if (pi)
                pi->pnext = NULL;
            delete pd;
        }
    }

   CBlockIndex* AddParent()
   {
       pindex->pprev = new CBlockIndex;
       pindex->pprev->pnext = pindex;
       pindex = pindex->pprev;
       return pindex;
   }
};

BOOST_AUTO_TEST_CASE(signer_reward_vote_result)
{
    {
        // Without any vote nor previous value, the result is 0
        CSignerRewardChain chain;
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        BOOST_CHECK_EQUAL(0, chain.pindexBest->signerRewardVoteResult.nCount);
        BOOST_CHECK_EQUAL(0, chain.pindexBest->signerRewardVoteResult.nAmount);
    }

    {
        // With a previous value, it doesn't change (each non-vote is a vote to keep the same parameters)
        CSignerRewardChain chain;
        chain.AddParent()->signerRewardVoteResult.Set(42, 521465);
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        BOOST_CHECK_EQUAL(    42, chain.pindexBest->signerRewardVoteResult.nCount);
        BOOST_CHECK_EQUAL(521465, chain.pindexBest->signerRewardVoteResult.nAmount);
    }

    {
        // In case of a tie (here 1000 votes to change and 1000 votes to keep the current value), the highest value wins
        CSignerRewardChain chain;
        chain.AddParent()->signerRewardVoteResult.Set(42, 1000);
        for (int i = 0; i < 1000; i++)
            chain.AddParent()->vote.signerReward.Set(12, 2000);
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        BOOST_CHECK_EQUAL(  42, chain.pindexBest->signerRewardVoteResult.nCount);
        BOOST_CHECK_EQUAL(2000, chain.pindexBest->signerRewardVoteResult.nAmount);
    }

    {
        // With 1001 votes to change, the result changes
        CSignerRewardChain chain;
        chain.AddParent()->signerRewardVoteResult.Set(42, 521465);
        for (int i = 0; i < 1001; i++)
            chain.AddParent()->vote.signerReward.Set(12, 12345);
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        BOOST_CHECK_EQUAL(    12, chain.pindexBest->signerRewardVoteResult.nCount);
        BOOST_CHECK_EQUAL( 12345, chain.pindexBest->signerRewardVoteResult.nAmount);
    }

    {
        // With a more complex median calculation
        CSignerRewardChain chain;
        chain.pindex->vote.signerReward.Set(10, 100);
        chain.AddParent()->signerRewardVoteResult.Set(5, 30);
        chain.pindex->vote.signerReward.Set(10, 100);
        for (int i = 0; i < 600; i++)
            chain.AddParent()->vote.signerReward.Set(6, -1);
        for (int i = 0; i < 400; i++)
            chain.AddParent()->vote.signerReward.Set(20, 10);
        for (int i = 0; i < 300; i++)
            chain.AddParent()->vote.signerReward.Set(-5, 15);
        for (int i = 0; i < 900; i++)
            chain.AddParent()->vote.signerReward.Set( 3,  3);
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        // So we have:
        // nCount: 2x10, 600x6, 400x20, 300x-1, 900x3 (only 698 count)
        // That means 698x3, 300x5, 600x6, 2x10, 400x20
        // The median is 6
        BOOST_CHECK_EQUAL(     6, chain.pindexBest->signerRewardVoteResult.nCount);
        // nReward: 2x100, 600x-1, 400x10, 700x15, 900x3 (only 698 count)
        // That means 698x3, 400x10, 300x15, 600x30, 2x100
        // The median is 10
        BOOST_CHECK_EQUAL(    10, chain.pindexBest->signerRewardVoteResult.nAmount);
    }

    {
        // Pre-4.0 blocks do not get any reward
        CSignerRewardChain chain;
        chain.pindex->nProtocolVersion = PROTOCOL_V2_0;
        chain.AddParent()->signerRewardVoteResult.Set(0, 0);
        for (int i = 0; i < 1001; i++)
            chain.AddParent()->vote.signerReward.Set(12, 12345);
        BOOST_CHECK(CalculateSignerRewardVoteResult(chain.pindexBest));
        BOOST_CHECK_EQUAL(     0, chain.pindexBest->signerRewardVoteResult.nCount);
        BOOST_CHECK_EQUAL(     0, chain.pindexBest->signerRewardVoteResult.nAmount);
    }
}

void NewBlockTip(CBlockIndex*& pindexBest)
{
    CBlockIndex *pindex = pindexBest;
    pindexBest = new CBlockIndex;
    pindexBest->pprev = pindex;
}

void CheckAsset(const CAsset& asset, uint32_t assetId, unsigned short confirmations, unsigned char m, unsigned char n,
                int64 maxTrade, int64 minTrade, uint8_t nUnitExponent)
{
    BOOST_CHECK_EQUAL(assetId, asset.nAssetId);
    BOOST_CHECK_EQUAL(confirmations, asset.nNumberOfConfirmations);
    BOOST_CHECK_EQUAL(m, asset.nRequiredDepositSigners);
    BOOST_CHECK_EQUAL(n, asset.nTotalDepositSigners);
    BOOST_CHECK_EQUAL(maxTrade, asset.GetMaxTrade());
    BOOST_CHECK_EQUAL(minTrade, asset.GetMinTrade());
    BOOST_CHECK_EQUAL(nUnitExponent, asset.nUnitExponent);
}

CAssetVote NewAssetVote(uint32_t assetId, unsigned short confirmations, unsigned char m, unsigned char n,
                        uint8_t maxTradeExpParam, uint8_t minTradeExpParam, uint8_t nUnitExponent)
{
    CAssetVote vote;

    vote.nAssetId = assetId;
    vote.nNumberOfConfirmations = confirmations;
    vote.nRequiredDepositSigners = m;
    vote.nTotalDepositSigners = n;
    vote.nMaxTradeExpParam = maxTradeExpParam;
    vote.nMinTradeExpParam = minTradeExpParam;
    vote.nUnitExponent = nUnitExponent;

    return vote;
}

BOOST_AUTO_TEST_CASE(asset_vote_result)
{
    CBlockIndex* pindexBest = new CBlockIndex;
    CBlockIndex* pindex = pindexBest;

    // asset 1
    const uint32_t ASSET_ID = 0x40000a83; // BTC string
    const uint16_t CONFIRMATIONS = 60;
    const uint8_t M = 3;
    const uint8_t N = 5;
    const uint8_t MAX_TRADE_EXP_PARAM = 91;
    const uint8_t MIN_TRADE_EXP_PARAM = 81;
    const int64 MAX_TRADE = pnExponentialSeries[MAX_TRADE_EXP_PARAM];
    const int64 MIN_TRADE = pnExponentialSeries[MIN_TRADE_EXP_PARAM];
    const uint8_t UNIT_EXPONENT = 6;

    // asset 2
    const uint32_t ASSET_ID_2 = 0x81083829; // ABC09
    const uint16_t CONFIRMATIONS_2 = 6;
    const uint8_t M_2 = 2;
    const uint8_t N_2 = 3;
    const uint8_t MAX_TRADE_EXP_PARAM_2 = 120;
    const uint8_t MIN_TRADE_EXP_PARAM_2 = 100;
    const int64 MAX_TRADE_2 = pnExponentialSeries[MAX_TRADE_EXP_PARAM_2];
    const int64 MIN_TRADE_2 = pnExponentialSeries[MIN_TRADE_EXP_PARAM_2];
    const uint8_t UNIT_EXPONENT_2 = 8;

    vector<CAsset> vAssets;
    map<uint32_t, CAsset> mapAssets;
    CAsset asset;

    // Check vote objects
    CVote vote;
    vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE_EXP_PARAM, MIN_TRADE_EXP_PARAM, UNIT_EXPONENT));
    vote.vAssetVote.push_back(NewAssetVote(ASSET_ID_2, CONFIRMATIONS_2, M_2, N_2, MAX_TRADE_EXP_PARAM_2, MIN_TRADE_EXP_PARAM_2, UNIT_EXPONENT_2));
    BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V2_0)); // not valid in old blocks
    BOOST_CHECK(vote.IsValidInBlock(PROTOCOL_V4_0)); // valid in new blocks
    // Add a duplicate vote for the same asset
    vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS+1, M, N, MAX_TRADE_EXP_PARAM+1, MIN_TRADE_EXP_PARAM+1, UNIT_EXPONENT));
    BOOST_CHECK(!vote.IsValidInBlock(PROTOCOL_V4_0)); // not valid due to duplicate vote

    // No vote, no assets
    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(0, vAssets.size());

    // Add a single vote
    pindex->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE_EXP_PARAM, MIN_TRADE_EXP_PARAM, UNIT_EXPONENT));
    BOOST_CHECK(pindex->vote.IsValidInBlock(PROTOCOL_V4_0));
    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(0, vAssets.size());
    BOOST_CHECK(CalculateVotedAssets(pindexBest));
    BOOST_CHECK_EQUAL(0, pindexBest->mapAssets.size());
    BOOST_CHECK_EQUAL(0, pindexBest->mapAssetsPrev.size());

    // Add some asset votes but not enough to win the vote
    for (int i = 0; i < ASSET_VOTES / 2 - 5; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS-1, M-1, N-1, MAX_TRADE_EXP_PARAM-1, MIN_TRADE_EXP_PARAM-1, UNIT_EXPONENT));
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(0, vAssets.size());
    BOOST_CHECK_EQUAL(0, pindexBest->mapAssets.size());
    BOOST_CHECK_EQUAL(0, pindexBest->mapAssetsPrev.size());

    // Add some asset votes for the same asset but different values
    for (int i = 0; i < 5; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS+1, M+1, N+1, MAX_TRADE_EXP_PARAM+1, MIN_TRADE_EXP_PARAM+1, UNIT_EXPONENT));
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(1, vAssets.size());
    CheckAsset(vAssets[0], ASSET_ID, CONFIRMATIONS-1, M-1, N-1, pnExponentialSeries[MAX_TRADE_EXP_PARAM-1], pnExponentialSeries[MIN_TRADE_EXP_PARAM-1], UNIT_EXPONENT);

    // Asset is not effective yet
    BOOST_CHECK(pindexBest->GetEffectiveAssets(mapAssets));
    BOOST_CHECK_EQUAL(0, mapAssets.size());

    // Asset should become effective after a delay
    for (int i = 0; i < VOTE_DELAY_BLOCKS; i++)
    {
        NewBlockTip(pindexBest);
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(pindexBest->GetEffectiveAssets(mapAssets));
    BOOST_CHECK_EQUAL(1, mapAssets.size());
    asset = CAsset(); // reset
    BOOST_CHECK(pindexBest->GetEffectiveAsset(ASSET_ID, asset));
    CheckAsset(asset, ASSET_ID, CONFIRMATIONS-1, M-1, N-1, pnExponentialSeries[MAX_TRADE_EXP_PARAM-1], pnExponentialSeries[MIN_TRADE_EXP_PARAM-1], UNIT_EXPONENT);

    // Add some normal asset votes to achieve a majority
    for (int i = 0; i < ASSET_VOTES / 2 + 1; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE_EXP_PARAM, MIN_TRADE_EXP_PARAM, UNIT_EXPONENT));
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(1, vAssets.size());
    CheckAsset(vAssets[0], ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE, MIN_TRADE, UNIT_EXPONENT);

    // Updated asset is not effective yet
    asset = CAsset(); // reset
    BOOST_CHECK(pindexBest->GetEffectiveAsset(ASSET_ID, asset));
    CheckAsset(asset, ASSET_ID, CONFIRMATIONS-1, M-1, N-1, pnExponentialSeries[MAX_TRADE_EXP_PARAM-1], pnExponentialSeries[MIN_TRADE_EXP_PARAM-1], UNIT_EXPONENT);

    for (int i = 0; i < VOTE_DELAY_BLOCKS; i++)
    {
        NewBlockTip(pindexBest);
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(pindexBest->GetEffectiveAssets(mapAssets));
    BOOST_CHECK_EQUAL(1, mapAssets.size());
    asset = CAsset(); // reset
    BOOST_CHECK(pindexBest->GetEffectiveAsset(ASSET_ID, asset));
    CheckAsset(asset, ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE, MIN_TRADE, UNIT_EXPONENT);

    // Add some more normal asset votes, the previous asset has a new maxTrade value and we add a new asset
    for (int i = 0; i < ASSET_VOTES / 2 + 1; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE_EXP_PARAM+1, MIN_TRADE_EXP_PARAM+1, UNIT_EXPONENT));
        pindexBest->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID_2, CONFIRMATIONS_2, M_2, N_2, MAX_TRADE_EXP_PARAM_2, MIN_TRADE_EXP_PARAM_2, UNIT_EXPONENT_2));
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(2, vAssets.size());
    BOOST_FOREACH(const CAsset& asset, vAssets)
    {
        if (asset.nAssetId == ASSET_ID)
            CheckAsset(asset, ASSET_ID, CONFIRMATIONS, M, N, pnExponentialSeries[MAX_TRADE_EXP_PARAM+1], pnExponentialSeries[MIN_TRADE_EXP_PARAM+1], UNIT_EXPONENT);
        else
            CheckAsset(asset, ASSET_ID_2, CONFIRMATIONS_2, M_2, N_2, MAX_TRADE_2, MIN_TRADE_2, UNIT_EXPONENT_2);
    }

    // new assets are not effective yet
    BOOST_CHECK(pindexBest->GetEffectiveAssets(mapAssets));
    BOOST_CHECK_EQUAL(1, mapAssets.size());

    for (int i = 0; i < VOTE_DELAY_BLOCKS; i++)
    {
        NewBlockTip(pindexBest);
        BOOST_CHECK(CalculateVotedAssets(pindexBest));
    }

    BOOST_CHECK(pindexBest->GetEffectiveAssets(mapAssets));
    BOOST_CHECK_EQUAL(2, mapAssets.size());
    asset = CAsset(); // reset
    BOOST_CHECK(pindexBest->GetEffectiveAsset(ASSET_ID, asset));
    CheckAsset(asset, ASSET_ID, CONFIRMATIONS, M, N, pnExponentialSeries[MAX_TRADE_EXP_PARAM+1], pnExponentialSeries[MIN_TRADE_EXP_PARAM+1], UNIT_EXPONENT);
    asset = CAsset(); // reset
    BOOST_CHECK(pindexBest->GetEffectiveAsset(ASSET_ID_2, asset));
    CheckAsset(asset, ASSET_ID_2, CONFIRMATIONS_2, M_2, N_2, MAX_TRADE_2, MIN_TRADE_2, UNIT_EXPONENT_2);

    // free memory
    while(pindexBest != NULL)
    {
        pindex = pindexBest;
        pindexBest = pindexBest->pprev;
        delete pindex;
    }
}

void AddSampleAssetVoteWithExponent(CBlockIndex* pindex, uint8_t nExponent)
{
    const uint32_t ASSET_ID = 0x40000a83; // BTC string
    const uint16_t CONFIRMATIONS = 60;
    const uint8_t M = 3;
    const uint8_t N = 5;
    const uint8_t MAX_TRADE_EXP_PARAM = 91;
    const uint8_t MIN_TRADE_EXP_PARAM = 81;
    pindex->vote.vAssetVote.push_back(NewAssetVote(ASSET_ID, CONFIRMATIONS, M, N, MAX_TRADE_EXP_PARAM, MIN_TRADE_EXP_PARAM, nExponent));
}

void CheckSampleAssetWithExponent(const CAsset& asset, uint8_t nUnitExponent)
{
    const uint32_t ASSET_ID = 0x40000a83; // BTC string
    const uint16_t CONFIRMATIONS = 60;
    const uint8_t M = 3;
    const uint8_t N = 5;
    const uint8_t MAX_TRADE_EXP_PARAM = 91;
    const uint8_t MIN_TRADE_EXP_PARAM = 81;
    const int64 MAX_TRADE = pnExponentialSeries[MAX_TRADE_EXP_PARAM];
    const int64 MIN_TRADE = pnExponentialSeries[MIN_TRADE_EXP_PARAM];

    BOOST_CHECK_EQUAL(ASSET_ID, asset.nAssetId);
    BOOST_CHECK_EQUAL(CONFIRMATIONS, asset.nNumberOfConfirmations);
    BOOST_CHECK_EQUAL(M, asset.nRequiredDepositSigners);
    BOOST_CHECK_EQUAL(N, asset.nTotalDepositSigners);
    BOOST_CHECK_EQUAL(MAX_TRADE, asset.GetMaxTrade());
    BOOST_CHECK_EQUAL(MIN_TRADE, asset.GetMinTrade());
    BOOST_CHECK_EQUAL(nUnitExponent, asset.nUnitExponent);
}


BOOST_AUTO_TEST_CASE(asset_vote_on_inexistant_asset_without_absolute_majority_on_exponent)
{
    CBlockIndex* pindexBest = new CBlockIndex;

    for (int i = 0; i < ASSET_VOTES / 2; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 4);
    }

    for (int i = 0; i < 10; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 5);
    }

    for (int i = 0; i < ASSET_VOTES / 2 - 10; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 6);
    }

    vector<CAsset> vAssets;
    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(0, vAssets.size());
}

BOOST_AUTO_TEST_CASE(asset_vote_on_inexistant_asset_with_absolute_majority_on_exponent)
{
    CBlockIndex* pindexBest = new CBlockIndex;
    for (int i = 0; i < ASSET_VOTES / 2 + 1; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 4);
    }

    for (int i = 0; i < 10; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 5);
    }

    for (int i = 0; i < ASSET_VOTES / 2 - 1 - 10; i++)
    {
        NewBlockTip(pindexBest);
        AddSampleAssetVoteWithExponent(pindexBest, 6);
    }

    vector<CAsset> vAssets;
    BOOST_CHECK(ExtractAssetVoteResult(pindexBest, vAssets));
    BOOST_CHECK_EQUAL(1, vAssets.size());
    CheckSampleAssetWithExponent(vAssets[0], 4);
}

int64 GetTime(int year, int month, int day, int hour, int min, int sec)
{
    tm t;
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return timegm(&t);
}

std::string TimeString(struct tm* timeinfo)
{
    char buffer[100];
    strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", timeinfo);
    std::string result(buffer);
    return result;
}

void CheckTimeEqual(time_t t1, time_t t2)
{
    string s1 = TimeString(gmtime(&t1));
    string s2 = TimeString(gmtime(&t2));
    BOOST_CHECK_EQUAL(s1, s2);
}

void CheckProtocolv4SwitchTime(int64 nVotePassTime, int64 nExpectedSwitchTime)
{
    int nVotesForNewProtocol = PROTOCOL_V4_SWITCH_REQUIRE_VOTES;
    int nVotesAgainstNewProtocol = PROTOCOL_V4_SWITCH_COUNT_VOTES - PROTOCOL_V4_SWITCH_REQUIRE_VOTES;
    int nVotes = nVotesForNewProtocol + nVotesAgainstNewProtocol;
    int nInterval = 60;

    CBlockIndex* pindexBest = new CBlockIndex;
    pindexBest->nTime = nVotePassTime - nVotes * nInterval;
    pindexBest->vote.nVersionVote = 0;
    BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V4_0));

    for (int i = 0; i < PROTOCOL_V4_SWITCH_COUNT_VOTES - PROTOCOL_V4_SWITCH_REQUIRE_VOTES; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = 0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V4_0));
    }

    for (int i = 0; i < PROTOCOL_V4_SWITCH_REQUIRE_VOTES; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = PROTOCOL_V4_0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V4_0));
    }
    BOOST_CHECK_EQUAL(pindexBest->nTime, nVotePassTime);

    bool fUpgraded = false;
    for (int i = 0; i < 30000; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = 0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        if (MustUpgradeProtocol(pindexBest, PROTOCOL_V4_0))
        {
            fUpgraded = true;
            break;
        }
    }

    BOOST_CHECK(fUpgraded);
    CheckTimeEqual(nExpectedSwitchTime, pindexBest->nTime);
}

BOOST_AUTO_TEST_CASE(protocol_v4_upgrade_2_weeks_after_vote)
{
    CheckProtocolv4SwitchTime(GetTime(2015, 12, 13, 10, 40, 12), GetTime(2015, 12, 27, 14,  0, 12));
    CheckProtocolv4SwitchTime(GetTime(2015, 12, 13, 14,  0,  0), GetTime(2015, 12, 27, 14,  0,  0));
    CheckProtocolv4SwitchTime(GetTime(2015, 12, 13, 14,  0,  1), GetTime(2015, 12, 28, 14,  0,  1));
    CheckProtocolv4SwitchTime(GetTime(2015, 12, 24, 23, 59, 59), GetTime(2016,  1,  8, 14,  0, 59));
    CheckProtocolv4SwitchTime(GetTime(2015, 12, 25,  0,  0,  0), GetTime(2016,  1,  8, 14,  0,  0));
}

void CheckProtocolv5SwitchTime(int64 nVotePassTime, int64 nExpectedSwitchTime)
{
    int nVotesForNewProtocol = PROTOCOL_V5_SWITCH_REQUIRE_VOTES;
    int nVotesAgainstNewProtocol = PROTOCOL_V5_SWITCH_COUNT_VOTES - PROTOCOL_V5_SWITCH_REQUIRE_VOTES;
    int nVotes = nVotesForNewProtocol + nVotesAgainstNewProtocol;
    int nInterval = 60;

    CBlockIndex* pindexBest = new CBlockIndex;
    pindexBest->nTime = nVotePassTime - nVotes * nInterval;
    pindexBest->vote.nVersionVote = 0;
    BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V4_0));

    for (int i = 0; i < PROTOCOL_V5_SWITCH_COUNT_VOTES - PROTOCOL_V5_SWITCH_REQUIRE_VOTES; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = 0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V5_0));
    }

    for (int i = 0; i < PROTOCOL_V5_SWITCH_REQUIRE_VOTES; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = PROTOCOL_V5_0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        BOOST_CHECK(!MustUpgradeProtocol(pindexBest, PROTOCOL_V5_0));
    }
    BOOST_CHECK_EQUAL(pindexBest->nTime, nVotePassTime);

    bool fUpgraded = false;
    for (int i = 0; i < 30000; i++)
    {
        NewBlockTip(pindexBest);
        pindexBest->vote.nVersionVote = 0;
        pindexBest->nTime = pindexBest->pprev->nTime + nInterval;
        if (MustUpgradeProtocol(pindexBest, PROTOCOL_V5_0))
        {
            fUpgraded = true;
            break;
        }
    }

    BOOST_CHECK(fUpgraded);
    CheckTimeEqual(nExpectedSwitchTime, pindexBest->nTime);
}

BOOST_AUTO_TEST_CASE(protocol_v5_upgrade_2_weeks_after_vote)
{
    CheckProtocolv5SwitchTime(GetTime(2015, 12, 13, 10, 40, 12), GetTime(2015, 12, 27, 14,  0, 12));
    CheckProtocolv5SwitchTime(GetTime(2015, 12, 13, 14,  0,  0), GetTime(2015, 12, 27, 14,  0,  0));
    CheckProtocolv5SwitchTime(GetTime(2015, 12, 13, 14,  0,  1), GetTime(2015, 12, 28, 14,  0,  1));
    CheckProtocolv5SwitchTime(GetTime(2015, 12, 24, 23, 59, 59), GetTime(2016,  1,  8, 14,  0, 59));
    CheckProtocolv5SwitchTime(GetTime(2015, 12, 25,  0,  0,  0), GetTime(2016,  1,  8, 14,  0,  0));
}

BOOST_AUTO_TEST_SUITE_END()
