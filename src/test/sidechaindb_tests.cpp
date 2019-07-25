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

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestingSetup)

bool ActivateSidechain(SidechainDB& scdbTest, const SidechainProposal& proposal, int nHeight)
{
    /* Activate a sidechain for testing purposes */
    unsigned int nActive = scdbTest.GetActiveSidechainCount();

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    if (!out.scriptPubKey.IsSidechainProposalCommit()) {
        return false;
    }

    uint256 hashBlock1 = GetRandHash();
    scdbTest.Update(nHeight, hashBlock1, scdbTest.GetHashBlockLastSeen(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    if (vActivation.size() != 1) {
        return false;
    }
    if (vActivation.front().proposal.GetHash() != proposal.GetHash())
    {
        return false;
    }

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
    int nHeightUpdate = nHeight + 1;
    uint256 hashPrev = hashBlock1;
    for (int i = 0; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        uint256 hashNew = GetRandHash();
        if (!scdbTest.Update(nHeightUpdate, hashNew, hashPrev, block.vtx.front()->vout)) {
            return false;
        }
        hashPrev = hashNew;
        nHeightUpdate++;
    }

    // Check activation status
    // Sidechain should have been removed from activation cache
    // Sidechain should be in ValidSidechains
    vActivation = scdbTest.GetSidechainActivationStatus();
    if (!vActivation.empty()) {
        return false;
    }

    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();
    return(vSidechain.size() == nActive + 1 && vSidechain.back() == proposal);
}

bool ActivateSidechain(SidechainDB& scdbTest, int nHeight = 0)
{
    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint256S("9fafdd046727ada4612cf9a860dd3e72ec0187bda31b1ef6fe84207b36537222");

    return ActivateSidechain(scdbTest, proposal, nHeight);
}

BOOST_AUTO_TEST_CASE(activate_single_sidechain)
{
    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);
    BOOST_CHECK(ActivateSidechain(scdbTest, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);
}

BOOST_AUTO_TEST_CASE(activate_multiple_sidechains)
{
    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);
    BOOST_CHECK(ActivateSidechain(scdbTest, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Proposal for a second sidechain
    SidechainProposal proposal;
    proposal.nVersion = 0;
    proposal.title = "sidechain2";
    proposal.description = "test";
    proposal.sidechainKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac"; // TODO
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = GetRandHash();

    BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 2);

    // Modify the proposal to create a third sidechain
    proposal.title = "sidechain3";

    BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 3);
}

BOOST_AUTO_TEST_CASE(activate_max_sidechains)
{
    SidechainDB scdbTest;

    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 0);

    SidechainProposal proposal;
    proposal.nVersion = 0;
    proposal.title = "sidechain";
    proposal.description = "test";
    proposal.sidechainKeyID = "c37afd89181060fa69deb3b26a0b95c02986ec78";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac"; // TODO
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r"; // TODO
    proposal.hashID1 = GetRandHash();
    proposal.hashID2 = GetRandHash();

    unsigned int nSidechains = 0;
    for (int i = 0; i < SIDECHAIN_ACTIVATION_MAX_ACTIVE; i++) {
        proposal.title = "sidechain" + std::to_string(i);

        BOOST_CHECK(ActivateSidechain(scdbTest, proposal, 0));

        nSidechains++;

        BOOST_CHECK(scdbTest.GetActiveSidechainCount() == nSidechains);
    }

    // Check that the maximum number have been activated
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == SIDECHAIN_ACTIVATION_MAX_ACTIVE);

    // Now try to activate one more than the max, it should be rejected.
    proposal.title = "one too many...";
    BOOST_CHECK(!ActivateSidechain(scdbTest, proposal, 0));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == SIDECHAIN_ACTIVATION_MAX_ACTIVE);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wtprime)
{
    // Test creating a WT^ and approving it with enough workscore
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    uint256 hashWTTest = GetRandHash();

    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = hashWTTest;
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wtTest.nWorkScore = i;
        wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wtTest}, nHeight);
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MultipleVerificationPeriods)
{
    // Test multiple verification periods, approve multiple WT^s on the
    // same sidechain
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // WT^ hash for first period
    uint256 hashWTTest1 = GetRandHash();

    // Verify first transaction, check work score
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = hashWTTest1;
    wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt1.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt1.nWorkScore = i;
        wt1.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        vWT.push_back(wt1);
        scdbTest.UpdateSCDBIndex(vWT, nHeight);
        nHeight++;
    }
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest1));

    // Create dummy coinbase tx
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, CScript() << OP_RETURN));

    uint256 hashBlock = GetRandHash();

    // Update scdbTest (will clear out old data from first period)
    scdbTest.Update(SIDECHAIN_VERIFICATION_PERIOD, hashBlock, scdbTest.GetHashBlockLastSeen(), mtx.vout);

    // WT^ hash for second period
    uint256 hashWTTest2 = GetRandHash();

    // Add new WT^
    std::vector<SidechainWTPrimeState> vWT;
    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = hashWTTest2;
    wt2.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt2.nSidechain = 0;
    wt2.nWorkScore = 1;
    vWT.push_back(wt2);
    scdbTest.UpdateSCDBIndex(vWT, 0);
    BOOST_CHECK(!scdbTest.CheckWorkScore(0, hashWTTest2));

    // Verify that scdbTest has updated to correct WT^
    const std::vector<SidechainWTPrimeState> vState = scdbTest.GetState(0);
    BOOST_CHECK(vState.size() == 1 && vState[0].hashWTPrime == hashWTTest2);

    // Give second transaction sufficient workscore and check work score
    nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt2.nWorkScore = i;
        wt2.nBlocksLeft--;
        vWT.push_back(wt2);
        scdbTest.UpdateSCDBIndex(vWT, nHeight);
        nHeight++;
    }
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashWTTest2));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_single)
{
    // Merkle tree based scdbTest update test with only scdbTest data (no LD)
    // in the tree, and a single WT^ to be updated.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Create scdbTest with initial WT^
    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.hashWTPrime = GetRandHash();
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt.nWorkScore = 1;
    wt.nSidechain = 0;

    vWT.push_back(wt);
    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    vWT.clear();
    wt.nWorkScore++;
    wt.nBlocksLeft--;
    vWT.push_back(wt);
    scdbTestCopy.UpdateSCDBIndex(vWT, 0);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msg;
    msg.nSidechain = 0;
    msg.hashWTPrime = wt.hashWTPrime;
    msg.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msg);

    scdbTest.AddSidechainNetworkUpdatePackage(updatePackage);

    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleSC)
{
    // TODO fix, does not actually have multiple sidechains
    // Merkle tree based scdbTest update test with multiple sidechains that each
    // have one WT^ to update. Only one WT^ out of the three will be updated.
    // This test ensures that nBlocksLeft is properly decremented even when a
    // WT^'s score is unchanged.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    wtTest.nBlocksLeft--;
    wtTest.nWorkScore++;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbTestCopy.UpdateSCDBIndex(vWT, 1);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = 0;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msgTest);

    scdbTest.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original scdbTest
    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleWT)
{
    // TODO fix, does not actually have multiple sidechains
    // Merkle tree based scdbTest update test with multiple sidechains and multiple
    // WT^(s) being updated. This tests that MT based scdbTest update will work if
    // work scores are updated for more than one sidechain per block.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // Add initial WT^s to scdbTest
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wtTest.nSidechain = 0;
    wtTest.nWorkScore = 1;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);

    scdbTest.UpdateSCDBIndex(vWT, 0);

    // Create a copy of the scdbTest to manipulate
    SidechainDB scdbTestCopy = scdbTest;

    // Update the scdbTest copy to get a new MT hash
    wtTest.nWorkScore++;
    wtTest.nBlocksLeft--;

    vWT.clear();
    vWT.push_back(wtTest);

    scdbTestCopy.UpdateSCDBIndex(vWT, 1);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = 0;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 2;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 2;
    updatePackage.vUpdate.push_back(msgTest);

    scdbTest.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original scdbTest
    BOOST_CHECK(scdbTest.UpdateSCDBMatchMT(2, scdbTestCopy.GetSCDBHash()));
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_create)
{
    // Create a deposit (and CTIP) for a single sidechain
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_sidechain)
{
    // Create a deposit (and CTIP) for multiple sidechains
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_deposits)
{
    // Create many deposits and make sure that single valid CTIP results
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create another deposit
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout.SetNull();

    CKey key2;
    CPubKey pubkey2;

    key2.MakeNewKey(true);
    pubkey2 = key2.GetPubKey();

    // User deposit data script
    CScript dataScript2 = CScript() << OP_RETURN << ToByteVector(pubkey2.GetID());

    mtx2.vout.push_back(CTxOut(CAmount(0), dataScript2));

    // Add deposit output
    mtx2.vout.push_back(CTxOut(25 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx2}, GetRandHash());

    // Check if we cached it
    vDeposit.clear();
    vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 2 && vDeposit.back().tx == mtx2);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip2;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip2));
    BOOST_CHECK(ctip2.out.hash == mtx2.GetHash());
    BOOST_CHECK(ctip2.out.n == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_multi_deposits_multi_sidechain)
{
    // Create many deposits and make sure that single valid CTIP results
    // for multiple sidechains.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_spend_wtprime)
{
    // Create a deposit (and CTIP) for a single sidechain,
    // and then spend it with a WT^
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create a WT^ that spends the CTIP
    CMutableTransaction wmtx;
    wmtx.nVersion = 2;
    wmtx.vin.push_back(CTxIn(ctip.out.hash, ctip.out.n));
    wmtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    // Give it sufficient work score
    SidechainWTPrimeState wt;
    uint256 hashBlind;
    BOOST_CHECK(CTransaction(wmtx).GetBWTHash(hashBlind));
    wt.hashWTPrime = hashBlind;
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wt.nWorkScore = i;
        wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wt}, nHeight);
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashBlind));

    // Spend the WT^
    BOOST_CHECK(scdbTest.SpendWTPrime(0, GetRandHash(), wmtx));

    // Check that the CTIP has been updated to the return amount from the WT^
    SidechainCTIP ctipFinal;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctipFinal));
    BOOST_CHECK(ctipFinal.out.hash == wmtx.GetHash());
    BOOST_CHECK(ctipFinal.out.n == 0);
}

BOOST_AUTO_TEST_CASE(sidechaindb_wallet_ctip_spend_wtprime_then_deposit)
{
    // Create a deposit (and CTIP) for a single sidechain, and then spend it
    // with a WT^. After doing that, create another deposit.
    SidechainDB scdbTest;

    BOOST_CHECK(ActivateSidechain(scdbTest));
    BOOST_CHECK(scdbTest.GetActiveSidechainCount() == 1);

    // TODO use the wallet function
    // Create deposit
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    CKey key;
    CPubKey pubkey;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // User deposit data script
    CScript dataScript = CScript() << OP_RETURN << ToByteVector(pubkey.GetID());

    mtx.vout.push_back(CTxOut(CAmount(0), dataScript));

    Sidechain sidechain;
    BOOST_CHECK(scdbTest.GetSidechain(0, sidechain));

    CScript sidechainScript;
    BOOST_CHECK(scdbTest.GetSidechainScript(0, sidechainScript));

    // Add deposit output
    mtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx}, GetRandHash());

    // Check if we cached it
    std::vector<SidechainDeposit> vDeposit = scdbTest.GetDeposits(0);
    BOOST_CHECK(vDeposit.size() == 1 && vDeposit.front().tx == mtx);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip));
    BOOST_CHECK(ctip.out.hash == mtx.GetHash());
    BOOST_CHECK(ctip.out.n == 1);

    // Create a WT^ that spends the CTIP
    CMutableTransaction wmtx;
    wmtx.nVersion = 2;
    wmtx.vin.push_back(CTxIn(ctip.out.hash, ctip.out.n));
    wmtx.vout.push_back(CTxOut(50 * CENT, sidechainScript));

    // Give it sufficient work score
    SidechainWTPrimeState wt;
    uint256 hashBlind;
    BOOST_CHECK(CTransaction(wmtx).GetBWTHash(hashBlind));
    wt.hashWTPrime = hashBlind;
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;
    wt.nSidechain = 0;
    int nHeight = 0;
    for (int i = 1; i <= SIDECHAIN_MIN_WORKSCORE; i++) {
        wt.nWorkScore = i;
        wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nHeight;
        scdbTest.UpdateSCDBIndex(std::vector<SidechainWTPrimeState>{wt}, nHeight);
        nHeight++;
    }

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdbTest.CheckWorkScore(0, hashBlind));

    // Spend the WT^
    BOOST_CHECK(scdbTest.SpendWTPrime(0, GetRandHash(), wmtx));

    // Check that the CTIP has been updated to the return amount from the WT^
    SidechainCTIP ctipFinal;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctipFinal));
    BOOST_CHECK(ctipFinal.out.hash == wmtx.GetHash());
    BOOST_CHECK(ctipFinal.out.n == 0);

    // Create another deposit
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout.SetNull();

    CKey key2;
    CPubKey pubkey2;

    key2.MakeNewKey(true);
    pubkey2 = key2.GetPubKey();

    // User deposit data script
    CScript dataScript2 = CScript() << OP_RETURN << ToByteVector(pubkey2.GetID());

    mtx2.vout.push_back(CTxOut(CAmount(0), dataScript2));

    // Add deposit output
    mtx2.vout.push_back(CTxOut(25 * CENT, sidechainScript));

    scdbTest.AddDeposits(std::vector<CTransaction>{mtx2}, GetRandHash());

    // Check if we cached it
    vDeposit.clear();
    vDeposit = scdbTest.GetDeposits(0);
    // Should now have 3 deposits cached (first deposit, WT^, this deposit)
    BOOST_CHECK(vDeposit.size() == 3 && vDeposit.back().tx == mtx2);

    // Compare with scdbTest CTIP
    SidechainCTIP ctip2;
    BOOST_CHECK(scdbTest.GetCTIP(0, ctip2));
    BOOST_CHECK(ctip2.out.hash == mtx2.GetHash());
    BOOST_CHECK(ctip2.out.n == 1);
}

BOOST_AUTO_TEST_CASE(IsCriticalHashCommit)
{
    // TODO
}

BOOST_AUTO_TEST_CASE(IsSCDBHashMerkleRootCommit)
{
    // TODO
}

BOOST_AUTO_TEST_CASE(IsWTPrimeHashCommit)
{
    // TODO test invalid
    // Test WT^ hash commitments for nSidechain 0-255 with random WT^ hashes
    for (unsigned int i = 0; i < 256; i++) {
        uint256 hashWTPrime = GetRandHash();
        uint8_t nSidechain = i;

        CBlock block;
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.SetNull();
        block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
        GenerateWTPrimeHashCommitment(block, hashWTPrime, nSidechain, Params().GetConsensus());

        uint256 hashWTPrimeFromCommit;
        uint8_t nSidechainFromCommit;
        BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsWTPrimeHashCommit(hashWTPrimeFromCommit, nSidechainFromCommit));

        BOOST_CHECK(hashWTPrime == hashWTPrimeFromCommit);
        BOOST_CHECK(nSidechain == nSidechainFromCommit);
    }
}

BOOST_AUTO_TEST_CASE(IsSidechainProposalCommit)
{
    // TODO test more proposals with different data, versions etc
    // TODO test invalid

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint256S("9fafdd046727ada4612cf9a860dd3e72ec0187bda31b1ef6fe84207b36537222");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());
}

BOOST_AUTO_TEST_CASE(IsSidechainActivationCommit)
{
    // TODO test more proposals with different data, versions etc
    // TODO test invalid

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.nVersion = 0;
    proposal.title = "Test";
    proposal.description = "Description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint256S("9fafdd046727ada4612cf9a860dd3e72ec0187bda31b1ef6fe84207b36537222");

    // Use the function from validation to generate the commit, and then
    // copy it from the block.
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    GenerateSidechainActivationCommitment(block, proposal.GetHash(), Params().GetConsensus());

    uint256 hashSidechain;
    BOOST_CHECK(block.vtx[0]->vout[0].scriptPubKey.IsSidechainActivationCommit(hashSidechain));

    BOOST_CHECK(hashSidechain == proposal.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
