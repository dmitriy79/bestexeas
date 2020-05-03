#include <boost/test/unit_test.hpp>

#include "serializable_tx_destination.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(serializable_tx_destination_tests)

static void CheckTxDestinationSerializeUnserialize(const CTxDestination& destination)
{
    CSerializableTxDestination serializableDestination(destination);
    CDataStream stream(SER_DISK, CLIENT_VERSION);
    stream << serializableDestination;

    vector<unsigned char> vch(stream.begin(), stream.end());
    CDataStream stream2(vch, SER_DISK, CLIENT_VERSION);
    CSerializableTxDestination result;
    stream2 >> result;
    CTxDestination resultDestination(result.GetTxDestination());
    BOOST_CHECK_EQUAL(destination.which(), resultDestination.which());
    switch(destination.which())
    {
        case 0:
            break;
        case 1:
            BOOST_CHECK(boost::get<CKeyID>(destination) == boost::get<CKeyID>(resultDestination));
            break;
        case 2:
            BOOST_CHECK(boost::get<CScriptID>(destination) == boost::get<CScriptID>(resultDestination));
            break;
    }
}

BOOST_AUTO_TEST_CASE(serialize)
{
    CheckTxDestinationSerializeUnserialize(CNoDestination());
    CheckTxDestinationSerializeUnserialize(CKeyID(uint160(123456)));
    CheckTxDestinationSerializeUnserialize(CScriptID(uint160(234567)));
}

BOOST_AUTO_TEST_SUITE_END()
