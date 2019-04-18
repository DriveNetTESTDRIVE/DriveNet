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

BOOST_AUTO_TEST_SUITE_END()
