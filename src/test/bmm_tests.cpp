// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/sigcache.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bmm_tests, TestChain100Setup)

/* TODO refactor along with DAG PR

BOOST_AUTO_TEST_CASE(bmm_checkCriticalHashValid)
{
    // Check that valid critical hash is added to ratchet
    SidechainDB scdb;

    // Create h* bribe script
    uint256 hashCritical = GetRandHash();
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(1) << ToByteVector(hashCritical);

    // Create dummy coinbase with h*
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was added
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical);
    BOOST_CHECK(it != mapLD.end());

}

BOOST_AUTO_TEST_CASE(bmm_checkCriticalHashInvalid)
{
    // Make sure that a invalid h* with a valid block number will
    // be rejected.
    SidechainDB scdb;

    // Create h* bribe script
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(21000) << 0x426974636F696E;

    // Create dummy coinbase with h* bribe
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was rejected, linking data should be empty
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();
    BOOST_CHECK(mapLD.empty());
}

BOOST_AUTO_TEST_CASE(bmm_checkBlockNumberValid)
{
    // Make sure that a valid h* with a valid block number
    // will be accepted.
    SidechainDB scdb;

    // We have to add the first h* to the ratchet so that
    // there is something to compare with.

    // Create h* bribe script
    uint256 hashCritical = GetRandHash();
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(1) << ToByteVector(hashCritical);

    // Create dummy coinbase with h* in output
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that first h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Now we add a second h* with a valid block number

    // Create second h* bribe script
    uint256 hashCritical2 = GetRandHash();
    CScript scriptPubKey2;
    scriptPubKey2 << OP_RETURN << CScriptNum::serialize(2) << ToByteVector(hashCritical2);

    // Update SCDB so that second h* is processed
    mtx.vout[0] = CTxOut(50 * CENT, scriptPubKey2);
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was added
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical2);
    BOOST_CHECK(it != mapLD.end());
}

BOOST_AUTO_TEST_CASE(bmm_checkBlockNumberInvalid)
{
    // Try to add a valid h* with an invalid block number
    // and make sure it is skipped.
    SidechainDB scdb;

    // We have to add the first h* to the ratchet so that
    // there is something to compare with.

    // Create first h* bribe script
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(10) << ToByteVector(GetRandHash());

    // Create dummy coinbase with h* in output
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that first h* is processed
    std::string strError = "";
    scdb.Update(0, GetRandHash(), mtx.vout, strError);

    // Now we add a second h* with an invalid block number

    // Create second h* bribe script
    uint256 hashCritical2 = GetRandHash();
    CScript scriptPubKey2;
    scriptPubKey2 << OP_RETURN << CScriptNum::serialize(100) << ToByteVector(hashCritical2);

    // Update SCDB so that second h* is processed
    CMutableTransaction mtx1;
    mtx1.nVersion = 1;
    mtx1.vin.resize(1);
    mtx1.vout.resize(1);
    mtx1.vin[0].scriptSig = CScript() << 486604899;
    mtx1.vout.push_back(CTxOut(50 * CENT, scriptPubKey2));

    scdb.Update(1, GetRandHash(), mtx1.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was rejected
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical2);
    BOOST_CHECK(it == mapLD.end());
}

*/

BOOST_AUTO_TEST_SUITE_END()
