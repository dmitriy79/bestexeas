
#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "main.h"
#include "wallet.h"
#include "util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(util_tests)

BOOST_AUTO_TEST_CASE(util_criticalsection)
{
    CCriticalSection cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while(0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest)
            break;

        BOOST_ERROR("break was swallowed!");
    } while(0);
}

BOOST_AUTO_TEST_CASE(util_MedianFilter)
{    
    CMedianFilter<int> filter(5, 15);

    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(20); // [15 20]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(30); // [15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 20);

    filter.input(3); // [3 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(7); // [3 7 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(18); // [3 7 18 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 18);

    filter.input(0); // [0 3 7 18 30]
    BOOST_CHECK_EQUAL(filter.median(), 7);
}

static const unsigned char ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7, 
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde, 
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12, 
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d, 
    0x5f
};
BOOST_AUTO_TEST_CASE(util_ParseHex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
    // Basic test vector
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());

    // Spaces between bytes must be supported
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + 5, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected, true),
        "");

    std::vector<unsigned char> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec, true),
        "04 67 8a fd b0");
}


BOOST_AUTO_TEST_CASE(util_DateTimeStrFormat)
{
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0), "01/01/70 00:00:00");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0x7FFFFFFF), "01/19/38 03:14:07");
    // Formats used within bitcoin
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 1317425777), "09/30/11 23:36:17");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M", 1317425777), "09/30/11 23:36");
}

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    ParseParameters(0, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(1, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(5, (char**)argv_test);
    // expectation: -ignored is ignored (program name argument), 
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    BOOST_CHECK(mapArgs.size() == 3 && mapMultiArgs.size() == 3);
    BOOST_CHECK(mapArgs.count("-a") && mapArgs.count("-b") && mapArgs.count("-ccc") 
                && !mapArgs.count("f") && !mapArgs.count("-d"));
    BOOST_CHECK(mapMultiArgs.count("-a") && mapMultiArgs.count("-b") && mapMultiArgs.count("-ccc") 
                && !mapMultiArgs.count("f") && !mapMultiArgs.count("-d"));

    BOOST_CHECK(mapArgs["-a"] == "" && mapArgs["-ccc"] == "multiple");
    BOOST_CHECK(mapMultiArgs["-ccc"].size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    mapArgs.clear();
    mapArgs["strtest1"] = "string...";
    // strtest2 undefined on purpose
    mapArgs["inttest1"] = "12345";
    mapArgs["inttest2"] = "81985529216486895";
    // inttest3 undefined on purpose
    mapArgs["booltest1"] = "";
    // booltest2 undefined on purpose
    mapArgs["booltest3"] = "0";
    mapArgs["booltest4"] = "1";

    BOOST_CHECK_EQUAL(GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest1"), true);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest2"), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest3"), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest4"), true);
}

BOOST_AUTO_TEST_CASE(util_WildcardMatch)
{
    BOOST_CHECK(WildcardMatch("127.0.0.1", "*"));
    BOOST_CHECK(WildcardMatch("127.0.0.1", "127.*"));
    BOOST_CHECK(WildcardMatch("abcdef", "a?cde?"));
    BOOST_CHECK(!WildcardMatch("abcdef", "a?cde??"));
    BOOST_CHECK(WildcardMatch("abcdef", "a*f"));
    BOOST_CHECK(!WildcardMatch("abcdef", "a*x"));
    BOOST_CHECK(WildcardMatch("", "*"));
}

BOOST_AUTO_TEST_CASE(util_FormatMoney)
{
    BOOST_CHECK_EQUAL(FormatMoney(0, false), "0.00");
    BOOST_CHECK_EQUAL(FormatMoney((COIN/10000)*123456789, false), "12345.6789");
    BOOST_CHECK_EQUAL(FormatMoney(COIN, true), "+1.00");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN, false), "-1.00");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN, true), "-1.00");

    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000000, false), "100000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000000, false), "10000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000000, false), "1000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000, false), "100000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000, false), "10000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000, false), "1000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100, false), "100.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10, false), "10.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN, false), "1.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10, false), "0.10");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100, false), "0.01");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000, false), "0.001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000, false), "0.0001");
}

BOOST_AUTO_TEST_CASE(util_ParseMoney)
{
    int64 ret = 0;
    BOOST_CHECK(ParseMoney("0.0", ret));
    BOOST_CHECK_EQUAL(ret, 0);

    BOOST_CHECK(ParseMoney("12345.6789", ret));
    BOOST_CHECK_EQUAL(ret, (COIN/10000)*123456789);

    BOOST_CHECK(ParseMoney("100000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000000);
    BOOST_CHECK(ParseMoney("10000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000000);
    BOOST_CHECK(ParseMoney("1000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000000);
    BOOST_CHECK(ParseMoney("100000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000);
    BOOST_CHECK(ParseMoney("10000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000);
    BOOST_CHECK(ParseMoney("1000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000);
    BOOST_CHECK(ParseMoney("100.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100);
    BOOST_CHECK(ParseMoney("10.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10);
    BOOST_CHECK(ParseMoney("1.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("0.1", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10);
    BOOST_CHECK(ParseMoney("0.01", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100);
    BOOST_CHECK(ParseMoney("0.001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000);
    BOOST_CHECK(ParseMoney("0.0001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000);

    // Attempted 63 bit overflow should fail
    BOOST_CHECK(!ParseMoney("92233720368.5478", ret));
}

BOOST_AUTO_TEST_CASE(util_IsHex)
{
    BOOST_CHECK(IsHex("00"));
    BOOST_CHECK(IsHex("00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHex("ff"));
    BOOST_CHECK(IsHex("FF"));

    BOOST_CHECK(!IsHex(""));
    BOOST_CHECK(!IsHex("0"));
    BOOST_CHECK(!IsHex("a"));
    BOOST_CHECK(!IsHex("eleven"));
    BOOST_CHECK(!IsHex("00xx00"));
    BOOST_CHECK(!IsHex("0x0000"));
}


BOOST_AUTO_TEST_CASE(util_AssetIds)
{
    // Check asset ids
    BOOST_CHECK_EQUAL(0x40000a83, EncodeAssetId("BTC"));
    BOOST_CHECK_EQUAL("BTC", AssetIdToStr(0x40000a83));
    BOOST_CHECK(IsValidAssetId(0x40000a83));
    BOOST_CHECK_EQUAL(0x81083829, EncodeAssetId("ABC09"));
    BOOST_CHECK_EQUAL("ABC09", AssetIdToStr(0x81083829));
    BOOST_CHECK(IsValidAssetId(0x81083829));
    BOOST_CHECK_EQUAL(0x01234567, EncodeAssetId(0x01234567));
    BOOST_CHECK_EQUAL(0x01234567, EncodeAssetId("ID0019088743"));
    BOOST_CHECK_EQUAL("ID0019088743", AssetIdToStr(0x01234567));
    BOOST_CHECK(IsValidAssetId(0x01234567));
    BOOST_CHECK_EQUAL(0x3FFFFFFF, EncodeAssetId(0x3FFFFFFF));
    BOOST_CHECK_EQUAL(0x3FFFFFFF, EncodeAssetId("ID1073741823"));
    BOOST_CHECK_EQUAL("ID1073741823", AssetIdToStr(0x3FFFFFFF));
    BOOST_CHECK(IsValidAssetId(0x3FFFFFFF));
    BOOST_CHECK_EQUAL(0x00000001, EncodeAssetId(0x00000001));
    BOOST_CHECK_EQUAL(0x00000001, EncodeAssetId("ID0000000001"));
    BOOST_CHECK_EQUAL("ID0000000001", AssetIdToStr(0x00000001));
    BOOST_CHECK(IsValidAssetId(0x00000001));

    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId(ASSET_ID_INVALID_STR));
    BOOST_CHECK(!IsValidAssetId(0xFFFFFFFF));
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID_STR, AssetIdToStr(0xFFFFFFFF));

    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId("ID0000000000"));
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId(0));
    BOOST_CHECK(!IsValidAssetId(0));
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID_STR, AssetIdToStr(0));

    BOOST_CHECK(!IsValidAssetId(0x80002503)); // 'BTC' as an alphanumeric id
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID_STR, AssetIdToStr(0x80002503));

    // Empty ids
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId(""));
    BOOST_CHECK(!IsValidAssetId(0x40000000));
    BOOST_CHECK(!IsValidAssetId(0x80000000));

    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId("ID9999999999")); // overflow
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId("ID5368709119")); // overflow
    BOOST_CHECK_EQUAL(ASSET_ID_INVALID, EncodeAssetId("ID1073741824")); // overflow
}

BOOST_AUTO_TEST_CASE(util_ExpSeries)
{
    for (int i = 0; i <= EXP_SERIES_MAX_PARAM; i++)
        BOOST_CHECK_EQUAL(i, ExponentialParameter(pnExponentialSeries[i]));

    BOOST_CHECK_EQUAL(0, ExponentialParameter(0L));
    BOOST_CHECK_EQUAL(1, ExponentialParameter(1L));
    BOOST_CHECK_EQUAL(9, ExponentialParameter(9L));
    BOOST_CHECK_EQUAL(18, ExponentialParameter(99L));
    BOOST_CHECK_EQUAL(55, ExponentialParameter(1555555L));
    BOOST_CHECK_EQUAL(112, ExponentialParameter(4500000000000L));
    BOOST_CHECK_EQUAL(122, ExponentialParameter(50000000000001L));
    BOOST_CHECK_EQUAL(122, ExponentialParameter(59999999999999L));
    BOOST_CHECK_EQUAL(162, ExponentialParameter(999900000000000000L));
    BOOST_CHECK_EQUAL(171, ExponentialParameter(9223372036854775807L));

    BOOST_CHECK_EQUAL(0, ConvertExpParameter(0, 8, 8));
    BOOST_CHECK_EQUAL(0, ConvertExpParameter(0, 4, 8));
    BOOST_CHECK_EQUAL(1, ConvertExpParameter(1, 8, 8));
    BOOST_CHECK_EQUAL(42, ConvertExpParameter(42, 0, 0));
    BOOST_CHECK_EQUAL(42, ConvertExpParameter(42, 8, 8));
    BOOST_CHECK_EQUAL(49, ConvertExpParameter(40, 0, 1));
    BOOST_CHECK_EQUAL(76, ConvertExpParameter(40, 4, 8));
    BOOST_CHECK_EQUAL(1, ConvertExpParameter(36, 8, 4));
    BOOST_CHECK_EQUAL(1, ConvertExpParameter(21, 8, 4));
    BOOST_CHECK_EQUAL(45, ConvertExpParameter(81, 8, 4));
    BOOST_CHECK_EQUAL(171, ConvertExpParameter(169, 4, 8));
    BOOST_CHECK_EQUAL(133, ConvertExpParameter(169, 8, 4));

    BOOST_CHECK_EQUAL(ExponentialParameter(1000000L), ConvertExpParameter(ExponentialParameter(100000000L), 8, 6));
    BOOST_CHECK_EQUAL(ExponentialParameter(500000000L), ConvertExpParameter(ExponentialParameter(50000L), 4, 8));
    BOOST_CHECK_EQUAL(ExponentialParameter(3L), ConvertExpParameter(ExponentialParameter(3000000000000000000L), 18, 0));
    BOOST_CHECK_EQUAL(ExponentialParameter(3000000000000000000L), ConvertExpParameter(ExponentialParameter(3L), 0, 18));
    BOOST_CHECK_EQUAL(ExponentialParameter(9000000000000000000L), ConvertExpParameter(ExponentialParameter(9000000000000000000L), 6, 8));
    BOOST_CHECK_EQUAL(ExponentialParameter(1L), ConvertExpParameter(ExponentialParameter(100), 8, 4));
    BOOST_CHECK_EQUAL(ExponentialParameter(100L), ConvertExpParameter(ExponentialParameter(1), 8, 10));


}

BOOST_AUTO_TEST_SUITE_END()
