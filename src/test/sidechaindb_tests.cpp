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

#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestChain100Setup)

// Activate a sidechain for testing purposes
bool ActivateSidechain()
{
    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKey = ""; // TODO
    proposal.sidechainHex = ""; // TODO
    proposal.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    if (!out.scriptPubKey.IsSidechainProposalCommit())
        return false;

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    if (vActivation.size() != 1)
        return false;
    if (vActivation.front().proposal.GetHash() != proposal.GetHash())
        return false;

    // Use the function from validation to generate the commit, and then
    // copy it from the block.
    // TODO do this for all of the other unit tests that test commitments
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    GenerateSidechainActivationCommitment(block, proposal.GetHash(), Params().GetConsensus());

    // Add votes until the sidechain is activated
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        scdb.Update(i, GetRandHash(), block.vtx.front()->vout, strError);
    }

    // Check activation status
    // Sidechain should have been removed from activation cache
    // Sidechain should be in ValidSidechains
    vActivation = scdb.GetSidechainActivationStatus();
    if (!vActivation.empty())
        return false;

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    return(vSidechain.size() == 1 && vSidechain.front() == proposal);
}

BOOST_AUTO_TEST_CASE(sidechaindb_isolated)
{
    BOOST_CHECK(ActivateSidechain());
    BOOST_CHECK(scdb.GetActiveSidechainCount() == 1);

    // Test SidechainDB without blocks
    uint256 hashWTTest = GetRandHash();

    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = hashWTTest;
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0 /* dummy sidechain number */;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wtTest.nWorkScore = i;
        wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        scdb.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wtTest}, nHeight);
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdb.CheckWorkScore(0, hashWTTest));

    // Reset SCDB after testing
    scdb.ResetWTPrimeState();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MultipleVerificationPeriods)
{
    // Test SCDB with multiple verification periods,
    // approve multiple WT^s on the same sidechain.

    // WT^ hash for first period
    uint256 hashWTTest1 = GetRandHash();

    // Verify first transaction, check work score
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = hashWTTest1;
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt1.nSidechain = 0 /* dummy sidechain number */;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt1.nWorkScore = i;
        wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        vWT.push_back(wt1);
        scdb.UpdateSCDBIndex(vWT, nHeight);
        nHeight++;
    }
    BOOST_CHECK(scdb.CheckWorkScore(0, hashWTTest1));

    // Create dummy coinbase tx
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, CScript() << OP_RETURN));

    uint256 hashBlock = GetRandHash();

    // Update SCDB (will clear out old data from first period)
    std::string strError = "";
    scdb.Update(SIDECHAIN_VERIFICATION_PERIOD, hashBlock, mtx.vout, strError);

    // WT^ hash for second period
    uint256 hashWTTest2 = GetRandHash();

    // Add new WT^
    std::vector<SidechainWTPrimeState> vWT;
    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = hashWTTest2;
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt2.nSidechain = 0 /* dummy sidechain number */;
    wt2.nWorkScore = 1;
    vWT.push_back(wt2);
    scdb.UpdateSCDBIndex(vWT, 0);
    BOOST_CHECK(!scdb.CheckWorkScore(0, hashWTTest2));

    // Verify that SCDB has updated to correct WT^
    const std::vector<SidechainWTPrimeState> vState = scdb.GetState(0);
    BOOST_CHECK(vState.size() == 1 && vState[0].hashWTPrime == hashWTTest2);

    // Give second transaction sufficient workscore and check work score
    nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt2.nWorkScore = i;
        wt2.nBlocksLeft--;
        vWT.push_back(wt2);
        scdb.UpdateSCDBIndex(vWT, nHeight);
        nHeight++;
    }
    BOOST_CHECK(scdb.CheckWorkScore(0, hashWTTest2));

    // Reset SCDB after testing
    scdb.ResetWTPrimeState();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_single)
{
    // Merkle tree based SCDB update test with only
    // SCDB data (no LD) in the tree, and a single
    // WT^ to be updated.

    // Create SCDB with initial WT^
    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.hashWTPrime = GetRandHash();
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt.nWorkScore = 1;
    wt.nSidechain = 0;

    vWT.push_back(wt);
    scdb.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    vWT.clear();
    wt.nWorkScore++;
    wt.nBlocksLeft--;
    vWT.push_back(wt);
    scdbCopy.UpdateSCDBIndex(vWT, 0);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msg;
    msg.nSidechain = 0;
    msg.hashWTPrime = wt.hashWTPrime;
    msg.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msg);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    BOOST_CHECK(scdb.UpdateSCDBMatchMT(2, scdbCopy.GetSCDBHash()));

    // Reset SCDB after testing
    scdb.ResetWTPrimeState();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleSC)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // that each have one WT^ to update. Only one WT^ out of the
    // three will be updated. This test ensures that nBlocksLeft is
    // properly decremented even when a WT^'s score is unchanged.

    // Add initial WT^s to SCDB
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0 /* dummy sidechain number */;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdb.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    wtTest.nBlocksLeft--;
    wtTest.nWorkScore++;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbCopy.UpdateSCDBIndex(vWT, 1);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = 0 /* dummy sidechain number */;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msgTest);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(scdb.UpdateSCDBMatchMT(2, scdbCopy.GetSCDBHash()));

    // Reset SCDB after testing
    scdb.ResetWTPrimeState();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleWT)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // and multiple WT^(s) being updated. This tests that MT based
    // SCDB update will work if work scores are updated for more
    // than one sidechain per block.

    // Add initial WT^s to SCDB
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0 /* dummy sidechain number */;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdb.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    wtTest.nWorkScore++;
    wtTest.nBlocksLeft--;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbCopy.UpdateSCDBIndex(vWT, 1);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = 0 /* dummy sidechain number */;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msgTest);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(scdb.UpdateSCDBMatchMT(2, scdbCopy.GetSCDBHash()));

    // Reset SCDB after testing
    scdb.ResetWTPrimeState();
}

BOOST_AUTO_TEST_SUITE_END()
