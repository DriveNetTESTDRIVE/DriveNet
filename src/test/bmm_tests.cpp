// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "random.h"
#include "sidechain.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bmm_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bmm_commit)
{
    // Create a BMM h* request transaction
    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    bytes << CScriptNum(0 /* dummy sidechain number */);
    bytes << CScriptNum(0 /* dummy prevblockref */);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 3;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = GetRandHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 102;

    // Add critical data
    mtx.criticalData = criticalData;

    // Create dummy coinbase
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 102;

    // Add dummy coinbase & critical data tx to block
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    // Generate commit
    GenerateCriticalHashCommitments(block, Params().GetConsensus());

    // Check that the commit has been generated
    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsCriticalHashCommit());
}


BOOST_AUTO_TEST_CASE(bmm_commit_format)
{
    // Test the IsBMMCommitment function with many different BMM requests
    CCriticalData bmm;

    // Null
    BOOST_CHECK(!bmm.IsBMMRequest());

    // Null bytes
    bmm.hashCritical = GetRandHash();
    BOOST_CHECK(!bmm.IsBMMRequest());

    // Null h*
    bmm.hashCritical.SetNull();
    bmm.bytes = std::vector<unsigned char> {0x00};
    BOOST_CHECK(!bmm.IsBMMRequest());

    // With valid h*, invalid bytes
    bmm.hashCritical = GetRandHash();
    BOOST_CHECK(!bmm.IsBMMRequest());

    // With invalid h*, valid bytes
    bmm.hashCritical.SetNull();

    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;
    bytes << CScriptNum(0 /* nSidechain */);
    bytes << CScriptNum(0 /* nPrevBlockRef */);
    bytes << ToByteVector(HexStr(std::string("fd3s")));

    bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

    BOOST_CHECK(!bmm.IsBMMRequest());

    // Valid
    bmm.hashCritical = GetRandHash();
    BOOST_CHECK(bmm.IsBMMRequest());

    // Valid nSidechain 0 - 255 and check decoded return by reference)
    for (unsigned int i = 0; i < 256; i++) {
        bytes.clear();

        bytes.resize(3);
        bytes[0] = 0x00;
        bytes[1] = 0xbf;
        bytes[2] = 0x00;
        bytes << CScriptNum(i /* nSidechain */);
        bytes << CScriptNum(0 /* nPrevBlockRef */);
        bytes << ToByteVector(HexStr(std::string("fd3s")));

        bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

        uint8_t nSidechain;
        uint16_t nPrevBlockRef;
        std::string strPrevBlock = "";
        BOOST_CHECK(bmm.IsBMMRequest(nSidechain, nPrevBlockRef, strPrevBlock));

        BOOST_CHECK(nSidechain == i);
        BOOST_CHECK(nPrevBlockRef == 0);
        BOOST_CHECK(strPrevBlock == "fd3s");
    }

    // Valid prevBlockRef 0 - 65535 with one sidechain number
    for (unsigned int y = 0; y < 65536; y++) {
        bytes.clear();

        bytes.resize(3);
        bytes[0] = 0x00;
        bytes[1] = 0xbf;
        bytes[2] = 0x00;
        bytes << CScriptNum(128 /* nSidechain */);
        bytes << CScriptNum(y /* nPrevBlockRef */);
        bytes << ToByteVector(HexStr(std::string("ella")));

        bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

        uint8_t nSidechain;
        uint16_t nPrevBlockRef;
        std::string strPrevBlock = "";
        BOOST_CHECK(bmm.IsBMMRequest(nSidechain, nPrevBlockRef, strPrevBlock));

        BOOST_CHECK(nSidechain == 128);
        BOOST_CHECK(nPrevBlockRef == y);
        BOOST_CHECK(strPrevBlock == "ella");
    }

    // Valid prevBlockRef 0 - 65535 with different sidechain numbers
    int x = 0;
    for (unsigned int y = 0; y < 65536; y++) {
        bytes.clear();

        bytes.resize(3);
        bytes[0] = 0x00;
        bytes[1] = 0xbf;
        bytes[2] = 0x00;
        bytes << CScriptNum(x /* nSidechain */);
        bytes << CScriptNum(y /* nPrevBlockRef */);
        bytes << ToByteVector(HexStr(std::string("fd3s")));

        bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

        uint8_t nSidechain;
        uint16_t nPrevBlockRef;
        std::string strPrevBlock = "";
        BOOST_CHECK(bmm.IsBMMRequest(nSidechain, nPrevBlockRef, strPrevBlock));

        BOOST_CHECK(nSidechain == x);
        BOOST_CHECK(nPrevBlockRef == y);
        BOOST_CHECK(strPrevBlock == "fd3s");

        // Loop through possible nSidechain numbers as we test the prevBlockRef
        x++;
        if (x == 256)
            x = 0;
    }

    // Invalid nSidechain
    bytes.clear();

    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;
    bytes << CScriptNum(1337 /* nSidechain */);
    bytes << CScriptNum(0 /* nPrevBlockRef */);
    bytes << ToByteVector(HexStr(std::string("fd3s")));

    bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

    BOOST_CHECK(!bmm.IsBMMRequest());

    // Invalid prevBlockRef
    bytes.clear();

    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;
    bytes << CScriptNum(86 /* nSidechain */);
    bytes << CScriptNum(888888 /* nPrevBlockRef */);
    bytes << ToByteVector(HexStr(std::string("fd3s")));

    bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

    BOOST_CHECK(!bmm.IsBMMRequest());

    // Invalid prev bytes - too few
    bytes.clear();

    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;
    bytes << CScriptNum(0 /* nSidechain */);
    bytes << CScriptNum(86 /* nPrevBlockRef */);
    bytes << ToByteVector(HexStr(std::string("btc")));

    bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

    BOOST_CHECK(!bmm.IsBMMRequest());

    // Invalid prev bytes - too many
    bytes.clear();

    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;
    bytes << CScriptNum(255 /* nSidechain */);
    bytes << CScriptNum(0 /* nPrevBlockRef */);
    bytes << ToByteVector(HexStr(std::string("The Times 03/Jan/2009 Chancellor on brink of second bailout for banks")));

    bmm.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());

    BOOST_CHECK(!bmm.IsBMMRequest());
}

BOOST_AUTO_TEST_SUITE_END()
