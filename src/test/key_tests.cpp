#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#include "key.h"
#include "base58.h"
#include "uint256.h"
#include "util.h"

using namespace std;

// BlockShares keys
static const string str8Secret1     ("6AB5RJLZnaEj6R2V8vpnN2UP9PJgMH1oifZCS2duhW4KxHceNee");
static const string str8Secret2     ("69zWmgDULAixiNHWizXbNYdtAUCR5FYpSXj1EYUNzjPPmPAUYem");
static const string str8Secret1C    ("PmVUWFXMi9hanu3ZBzZuDoVrk1T2gVgZCuB7TUDubmFgeqbNPhXL");
static const string str8Secret2C    ("PkgrQC63dbvCuawtCLv2i6dnpo3PFmDSPgcQ2mur5p5qUbUoEfyH");

// BlockCredits keys
static const string strCSecret1     ("6C7pbXnbdzuxEEcC3WSnKM5A4VvPzyrgWJdJgBKohA4iSsjFoWx");
static const string strCSecret2     ("6BwFwufWBbQBrBsDda9bKsEf5ap8ixPhEAo7UhAGzPPnFyKZ4yc");
static const string strCSecret1C    ("Pv575QtFtJcimR4Kk4xo1ybLzSegWuxHNLoF1va16F1PwAhBCTtx");
static const string strCSecret2C    ("PuGUyMSwokqLt6xekRJvWGjH5EF36BVAZ8EXbEFwaHqYkveSG4nK");

// The resulting BKS addresses for the above keys
static const CBitcoinAddress addr81 ("8ZJqrcmJCfNb9DVudDDWBduKRpnLqca5vq");
static const CBitcoinAddress addr82 ("8VeSFyegJHg2bUBpnwpN14akHm53knrSpV");
static const CBitcoinAddress addr81C("8Z6Y2kXWmci4wWFg9FW9xC9hxTjs72bwyS");
static const CBitcoinAddress addr82C("8S5p4yC31f1qL3KUiqSPaRgE5eZt5C1muQ");

// The resulting BKC addresses for the above keys
static const CBitcoinAddress addrC1 ("CagshhkBJU1MKYtmsQYh2tdBisMmu6GRwy");
static const CBitcoinAddress addrC2 ("CX2U74dZQ6Jnmoah399YrKJcaoeUmzxr6P");
static const CBitcoinAddress addrC1C("CaUZsqWPsRLq7qeYPSqLoSsaFWKJDzgqXF");
static const CBitcoinAddress addrC2C("CTTqv4Av7TebWNiLy2maRgQ6Nh9KCEqZWD");

static const string strAddressBad("1HV9Lc3sNHZxwj4Zk6fB38tEmBryq2cBiF");


#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(uint256 privkey)
{
    CSecret secret;
    secret.resize(32);
    memcpy(&secret[0], &privkey, 32);
    vector<unsigned char> sec;
    sec.resize(32);
    memcpy(&sec[0], &secret[0], 32);
    printf("  * secret (hex): %s\n", HexStr(sec).c_str());

    for (int nCompressed=0; nCompressed<2; nCompressed++)
    {
        bool fCompressed = nCompressed == 1;
        printf("  * %s:\n", fCompressed ? "compressed" : "uncompressed");
        CBitcoinSecret bsecret;
        bsecret.SetSecret(secret, fCompressed, '8');
        printf("    * secret (base58, BKS): %s\n", bsecret.ToString().c_str());
        bsecret.SetSecret(secret, fCompressed, 'C');
        printf("    * secret (base58, BKC): %s\n", bsecret.ToString().c_str());
        CKey key;
        key.SetSecret(secret, fCompressed);
        CPubKey pubKey = key.GetPubKey();
        printf("    * pubkey (hex): %s\n", HexStr(pubKey.Raw()).c_str());
        printf("    * address (base58, BKS): %s\n", CBitcoinAddress(pubKey.GetID(), '8').ToString().c_str());
        printf("    * address (base58, BKC): %s\n", CBitcoinAddress(pubKey.GetID(), 'C').ToString().c_str());
    }
}
#endif


BOOST_AUTO_TEST_SUITE(key_tests)

BOOST_AUTO_TEST_CASE(key_test_8)
{
    /*
    dumpKeyInfo(uint256(1000));
    dumpKeyInfo(uint256(2000));
    */
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK( bsecret1.SetString (str8Secret1,   '8'));
    BOOST_CHECK( bsecret2.SetString (str8Secret2,   '8'));
    BOOST_CHECK( bsecret1C.SetString(str8Secret1C,  '8'));
    BOOST_CHECK( bsecret2C.SetString(str8Secret2C,  '8'));
    BOOST_CHECK(!baddress1.SetString(strAddressBad, '8'));

    bool fCompressed;
    CSecret secret1  = bsecret1.GetSecret (fCompressed);
    BOOST_CHECK(fCompressed == false);
    CSecret secret2  = bsecret2.GetSecret (fCompressed);
    BOOST_CHECK(fCompressed == false);
    CSecret secret1C = bsecret1C.GetSecret(fCompressed);
    BOOST_CHECK(fCompressed == true);
    CSecret secret2C = bsecret2C.GetSecret(fCompressed);
    BOOST_CHECK(fCompressed == true);

    BOOST_CHECK(secret1 == secret1C);
    BOOST_CHECK(secret2 == secret2C);

    CKey key1, key2, key1C, key2C;
    key1.SetSecret(secret1, false);
    key2.SetSecret(secret2, false);
    key1C.SetSecret(secret1, true);
    key2C.SetSecret(secret2, true);

    BOOST_CHECK(addr81.Get()  == CTxDestination(key1.GetPubKey().GetID()));
    BOOST_CHECK(addr82.Get()  == CTxDestination(key2.GetPubKey().GetID()));
    BOOST_CHECK(addr81C.Get() == CTxDestination(key1C.GetPubKey().GetID()));
    BOOST_CHECK(addr82C.Get() == CTxDestination(key2C.GetPubKey().GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( key1.Verify(hashMsg, sign1));
        BOOST_CHECK(!key1.Verify(hashMsg, sign2));
        BOOST_CHECK( key1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!key1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!key2.Verify(hashMsg, sign1));
        BOOST_CHECK( key2.Verify(hashMsg, sign2));
        BOOST_CHECK(!key2.Verify(hashMsg, sign1C));
        BOOST_CHECK( key2.Verify(hashMsg, sign2C));

        BOOST_CHECK( key1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!key1C.Verify(hashMsg, sign2));
        BOOST_CHECK( key1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!key1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!key2C.Verify(hashMsg, sign1));
        BOOST_CHECK( key2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!key2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( key2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.SetCompactSignature (hashMsg, csign1));
        BOOST_CHECK(rkey2.SetCompactSignature (hashMsg, csign2));
        BOOST_CHECK(rkey1C.SetCompactSignature(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.SetCompactSignature(hashMsg, csign2C));


        BOOST_CHECK(rkey1.GetPubKey()  == key1.GetPubKey());
        BOOST_CHECK(rkey2.GetPubKey()  == key2.GetPubKey());
        BOOST_CHECK(rkey1C.GetPubKey() == key1C.GetPubKey());
        BOOST_CHECK(rkey2C.GetPubKey() == key2C.GetPubKey());
    }
}

BOOST_AUTO_TEST_CASE(key_test_C)
{
    /*
    dumpKeyInfo(uint256(1000));
    dumpKeyInfo(uint256(2000));
    */
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK( bsecret1.SetString (strCSecret1,   'C'));
    BOOST_CHECK( bsecret2.SetString (strCSecret2,   'C'));
    BOOST_CHECK( bsecret1C.SetString(strCSecret1C,  'C'));
    BOOST_CHECK( bsecret2C.SetString(strCSecret2C,  'C'));
    BOOST_CHECK(!baddress1.SetString(strAddressBad, 'C'));

    bool fCompressed;
    CSecret secret1  = bsecret1.GetSecret (fCompressed);
    BOOST_CHECK(fCompressed == false);
    CSecret secret2  = bsecret2.GetSecret (fCompressed);
    BOOST_CHECK(fCompressed == false);
    CSecret secret1C = bsecret1C.GetSecret(fCompressed);
    BOOST_CHECK(fCompressed == true);
    CSecret secret2C = bsecret2C.GetSecret(fCompressed);
    BOOST_CHECK(fCompressed == true);

    BOOST_CHECK(secret1 == secret1C);
    BOOST_CHECK(secret2 == secret2C);

    CKey key1, key2, key1C, key2C;
    key1.SetSecret(secret1, false);
    key2.SetSecret(secret2, false);
    key1C.SetSecret(secret1, true);
    key2C.SetSecret(secret2, true);

    BOOST_CHECK(addrC1.Get()  == CTxDestination(key1.GetPubKey().GetID()));
    BOOST_CHECK(addrC2.Get()  == CTxDestination(key2.GetPubKey().GetID()));
    BOOST_CHECK(addrC1C.Get() == CTxDestination(key1C.GetPubKey().GetID()));
    BOOST_CHECK(addrC2C.Get() == CTxDestination(key2C.GetPubKey().GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( key1.Verify(hashMsg, sign1));
        BOOST_CHECK(!key1.Verify(hashMsg, sign2));
        BOOST_CHECK( key1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!key1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!key2.Verify(hashMsg, sign1));
        BOOST_CHECK( key2.Verify(hashMsg, sign2));
        BOOST_CHECK(!key2.Verify(hashMsg, sign1C));
        BOOST_CHECK( key2.Verify(hashMsg, sign2C));

        BOOST_CHECK( key1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!key1C.Verify(hashMsg, sign2));
        BOOST_CHECK( key1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!key1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!key2C.Verify(hashMsg, sign1));
        BOOST_CHECK( key2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!key2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( key2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.SetCompactSignature (hashMsg, csign1));
        BOOST_CHECK(rkey2.SetCompactSignature (hashMsg, csign2));
        BOOST_CHECK(rkey1C.SetCompactSignature(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.SetCompactSignature(hashMsg, csign2C));


        BOOST_CHECK(rkey1.GetPubKey()  == key1.GetPubKey());
        BOOST_CHECK(rkey2.GetPubKey()  == key2.GetPubKey());
        BOOST_CHECK(rkey1C.GetPubKey() == key1C.GetPubKey());
        BOOST_CHECK(rkey2C.GetPubKey() == key2C.GetPubKey());
    }
}

BOOST_AUTO_TEST_CASE(key_test2)
{
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C;
    // New encoding for BlockShares
    BOOST_CHECK( bsecret1.SetString (str8Secret1,  '8'));
    BOOST_CHECK( bsecret2.SetString (str8Secret2,  '8'));
    BOOST_CHECK( bsecret1C.SetString(str8Secret1C, '8'));
    BOOST_CHECK( bsecret2C.SetString(str8Secret2C, '8'));
    // Set BlockShares private key with BlockCredits unit, should fail
    BOOST_CHECK(!bsecret1.SetString (str8Secret1,  'C'));
    BOOST_CHECK(!bsecret2.SetString (str8Secret2,  'C'));
    BOOST_CHECK(!bsecret1C.SetString(str8Secret1C, 'C'));
    BOOST_CHECK(!bsecret2C.SetString(str8Secret2C, 'C'));

    // New encoding for BlockCredits
    BOOST_CHECK( bsecret1.SetString (strCSecret1,  'C'));
    BOOST_CHECK( bsecret2.SetString (strCSecret2,  'C'));
    BOOST_CHECK( bsecret1C.SetString(strCSecret1C, 'C'));
    BOOST_CHECK( bsecret2C.SetString(strCSecret2C, 'C'));
    // Set BlockCredits private key with BlockShares unit, should fail
    BOOST_CHECK(!bsecret1.SetString (strCSecret1,  '8'));
    BOOST_CHECK(!bsecret2.SetString (strCSecret2,  '8'));
    BOOST_CHECK(!bsecret1C.SetString(strCSecret1C, '8'));
    BOOST_CHECK(!bsecret2C.SetString(strCSecret2C, '8'));

    // Check the CBitcoinSecret
    bool fCompressed;
    CSecret secret = bsecret1.GetSecret(fCompressed);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, '8').ToString() == str8Secret1);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, 'C').ToString() == strCSecret1);
    secret = bsecret2.GetSecret(fCompressed);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, '8').ToString() == str8Secret2);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, 'C').ToString() == strCSecret2);
    secret = bsecret1C.GetSecret(fCompressed);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, '8').ToString() == str8Secret1C);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, 'C').ToString() == strCSecret1C);
    secret = bsecret2C.GetSecret(fCompressed);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, '8').ToString() == str8Secret2C);
    BOOST_CHECK(CBitcoinSecret(secret, fCompressed, 'C').ToString() == strCSecret2C);
}

BOOST_AUTO_TEST_SUITE_END()
