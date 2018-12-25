// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "validation.h"

#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sidechainactivation_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(sidechainproposal_single)
{
    // Test adding one proposal to scdb

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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    uint256 hash = GetRandHash();
    scdb.Update(0, hash, std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainproposal_multiple)
{
    // Test adding multiple proposal to scdb

    // Create sidechain proposal
    SidechainProposal proposal1;
    proposal1.title = "test1";
    proposal1.description = "description";
    proposal1.sidechainKey = ""; // TODO
    proposal1.sidechainHex = ""; // TODO
    proposal1.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal1.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    // Update SCDB to add the proposal
    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    // Create sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKey = ""; // TODO
    proposal2.sidechainHex = ""; // TODO
    proposal2.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    CTxOut out2;
    out2.scriptPubKey = proposal2.GetScript();
    out2.nValue = 50 * CENT;

    BOOST_CHECK(out2.scriptPubKey.IsSidechainProposalCommit());

    // Update SCDB to add the proposal
    scdb.Update(1, GetRandHash(), std::vector<CTxOut>{out2}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 2) &&
            (vActivation.front().proposal.GetHash() == proposal1.GetHash()) &&
            (vActivation.back().proposal.GetHash() == proposal2.GetHash()));

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainproposal_limit)
{
    // Test adding the maximum number of sidechain proposals
}

BOOST_AUTO_TEST_CASE(sidechainproposal_perblocklimit)
{
    // Make sure multiple sidechain proposals in one block will be rejected.

    // Creat sidechain proposal
    SidechainProposal proposal1;
    proposal1.title = "test1";
    proposal1.description = "description";
    proposal1.sidechainKey = ""; // TODO
    proposal1.sidechainHex = ""; // TODO
    proposal1.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal1.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    // Create sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKey = ""; // TODO
    proposal2.sidechainHex = ""; // TODO
    proposal2.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    CTxOut out2;
    out2.scriptPubKey = proposal2.GetScript();
    out2.nValue = 50 * CENT;

    BOOST_CHECK(out2.scriptPubKey.IsSidechainProposalCommit());

    // Update SCDB to add the proposal
    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out, out2}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    // Nothing should have been added
    BOOST_CHECK(vActivation.empty());

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_invalid)
{
    // Invalid name
    // Invalid description
    // Invalid private key
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate)
{
    // Test adding one proposal to scdb and activating it

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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));


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
    BOOST_CHECK(vActivation.empty());

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 1 && vSidechain.front() == proposal);

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate_multi)
{
    // Test adding two proposal to scdb and activating them, in a single voting
    // period

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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    // Create another sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKey = ""; // TODO
    proposal2.sidechainHex = ""; // TODO
    proposal2.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    out.scriptPubKey = proposal2.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    scdb.Update(1, GetRandHash(), std::vector<CTxOut>{out}, strError);

    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 2) && (vActivation.back().proposal.GetHash() == proposal2.GetHash()));

    // Use the function from validation to generate the commit, and then
    // copy it from the block.
    // TODO do this for all of the other unit tests that test commitments
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    // Generate commit for proposal 1
    GenerateSidechainActivationCommitment(block, proposal.GetHash(), Params().GetConsensus());
    // Generate commit for proposal 2
    GenerateSidechainActivationCommitment(block, proposal2.GetHash(), Params().GetConsensus());

    // Add votes until the sidechain is activated
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        scdb.Update(i, GetRandHash(), block.vtx.front()->vout, strError);
    }

    // Check activation status
    // Sidechains should have been removed from activation cache
    // Sidechains should be in ValidSidechains
    vActivation = scdb.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 2 && vSidechain.front() == proposal && vSidechain.back() == proposal2);

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate_multi_seperate)
{
    // Test adding two proposal to scdb and activating them, in seperate voting
    // periods

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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    // Use the function from validation to generate the commit, and then
    // copy it from the block.
    // TODO do this for all of the other unit tests that test commitments
    CBlock block;
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    // Generate commit for proposal 1
    GenerateSidechainActivationCommitment(block, proposal.GetHash(), Params().GetConsensus());

    // Add votes until the sidechain is activated
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        scdb.Update(i, GetRandHash(), block.vtx.front()->vout, strError);
    }

    // Check activation status
    // Sidechain should have been removed from activation cache
    // Sidechain should be in ValidSidechains
    vActivation = scdb.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    // Proposal 1 should have activated
    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 1 && vSidechain.front() == proposal);

    // Create another sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKey = ""; // TODO
    proposal2.sidechainHex = ""; // TODO
    proposal2.sidechainPriv = ""; // TODO

    // Create transaction output with sidechain proposal
    out.scriptPubKey = proposal2.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    scdb.Update(1, GetRandHash(), std::vector<CTxOut>{out}, strError);

    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal2.GetHash()));

    // Generate commit for proposal 2
    block.vtx.clear();
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    GenerateSidechainActivationCommitment(block, proposal2.GetHash(), Params().GetConsensus());

    // Add votes until the sidechain is activated
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        scdb.Update(i, GetRandHash(), block.vtx.front()->vout, strError);
    }

    // Now proposal 2 should be removed from the activation cache and should
    // be in the valid sidechain vector
    vActivation = scdb.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    vSidechain = scdb.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 2 && vSidechain.front() == proposal && vSidechain.back() == proposal2);

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_failActivation)
{
    // Test adding one proposal to scdb and fail to activate it

    // Creat sidechain proposal
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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

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
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE / 2; i++) {
        scdb.Update(i, GetRandHash(), block.vtx.front()->vout, strError);
    }

    // Check activation status
    // Sidechain should still be in activation cache
    // Sidechain should not be in ValidSidechains
    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();

    BOOST_CHECK(vSidechain.empty());

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_pruneRejected)
{
    // Test that sidechains which have no chance of success (based on their
    // rejection count) are pruned from the activation cache.

    // Creat sidechain proposal
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

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    std::string strError = "";
    scdb.Update(0, GetRandHash(), std::vector<CTxOut>{out}, strError);

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdb.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    // Pass coinbase without sidechain activation commit into SCDB enough times
    // that the proposal is rejected and pruned.
    out.scriptPubKey = OP_TRUE;
    out.nValue = 50 * CENT;

    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_FAILURES + 1; i++) {
        vActivation = scdb.GetSidechainActivationStatus();
        scdb.Update(i, GetRandHash(), std::vector<CTxOut>{out}, strError);
    }

    // Check activation status
    // Sidechain should have been pruned from activation cache
    // Sidechain should not be in ValidSidechains
    std::vector<SidechainActivationStatus> vActivationFinal;
    vActivationFinal = scdb.GetSidechainActivationStatus();
    BOOST_CHECK(vActivationFinal.empty());

    std::vector<Sidechain> vSidechain = scdb.GetActiveSidechains();
    BOOST_CHECK(vSidechain.empty());

    scdb.ResetWTPrimeState();
    scdb.ResetSidechains();
}

BOOST_AUTO_TEST_CASE(sidechainactivation_duplicateOfActivated)
{
    // Test proposing a sidechain that is an exact duplicate of a sidechain
    // that has already activated. Should be rejected.
}

BOOST_AUTO_TEST_SUITE_END()
