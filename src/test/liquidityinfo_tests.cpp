#include <boost/test/unit_test.hpp>
#include <algorithm>

#include "liquidityinfo.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(liquidityinfo_tests)

BOOST_AUTO_TEST_CASE(liquidityinfo_validity)
{
    CLiquidityInfo baseInfo;

    baseInfo.cUnit = 'C';
    baseInfo.nTime = GetAdjustedTime();
    baseInfo.nBuyAmount = 0;
    baseInfo.nSellAmount = 0;
    baseInfo.sIdentifier = "";

    BOOST_CHECK(baseInfo.IsValid());

    {
        CLiquidityInfo info(baseInfo);
        info.cUnit = '?';
        BOOST_CHECK(!info.IsValid());
    }

    {
        CLiquidityInfo info(baseInfo);
        info.cUnit = '8';
        BOOST_CHECK(!info.IsValid());
    }

    {
        CLiquidityInfo info(baseInfo);
        info.sIdentifier = "foo";
        BOOST_CHECK(info.IsValid());
    }

    {
        CLiquidityInfo info(baseInfo);
        info.sIdentifier = "\n";
        BOOST_CHECK(!info.IsValid());
    }

    {
        CLiquidityInfo info(baseInfo);
        info.sIdentifier = string("\0", 1);
        BOOST_CHECK(!info.IsValid());
    }

    {
        CLiquidityInfo info(baseInfo);
        info.sIdentifier = string(255, ' ');
        BOOST_CHECK(info.IsValid());
        info.sIdentifier += ' ';
        BOOST_CHECK(!info.IsValid());
    }

}

BOOST_AUTO_TEST_SUITE_END()
