// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "validation.h"

#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>


// TODO refactor.
// Combine this file with sidechaindb_tests where we have a convenient sidechain
// activation function and more sidechain activation tests. Sidechain activation
// is a fundamental part of SCDB anyways.


BOOST_FIXTURE_TEST_SUITE(sidechainactivation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(sidechainproposal_single)
{
    // Test adding one proposal to scdbTest
    SidechainDB scdbTest;

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash = GetRandHash();
    scdbTest.Update(0, hash, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));
}

BOOST_AUTO_TEST_CASE(sidechainproposal_multiple)
{
    // Test adding multiple proposal to scdbTest
    SidechainDB scdbTest;

    // Create sidechain proposal
    SidechainProposal proposal1;
    proposal1.title = "test1";
    proposal1.description = "description";
    proposal1.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal1.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal1.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal1.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal1.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal1.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    // Update scdbTest to add the proposal
    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    // Create sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal2.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal2.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal2.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out2;
    out2.scriptPubKey = proposal2.GetScript();
    out2.nValue = 50 * CENT;

    BOOST_CHECK(out2.scriptPubKey.IsSidechainProposalCommit());

    // Update scdbTest to add the proposal
    scdbTest.Update(1, GetRandHash(), hash1, std::vector<CTxOut>{out2});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 2) &&
            (vActivation.front().proposal.GetHash() == proposal1.GetHash()) &&
            (vActivation.back().proposal.GetHash() == proposal2.GetHash()));
}

BOOST_AUTO_TEST_CASE(sidechainproposal_limit)
{
    // Test adding the maximum number of sidechain proposals
}

BOOST_AUTO_TEST_CASE(sidechainproposal_perblocklimit)
{
    // Make sure multiple sidechain proposals in one block will be rejected.
    SidechainDB scdbTest;

    // Creat sidechain proposal
    SidechainProposal proposal1;
    proposal1.title = "test1";
    proposal1.description = "description";
    proposal1.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal1.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal1.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal1.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal1.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal1.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    // Create sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal2.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal2.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal2.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out2;
    out2.scriptPubKey = proposal2.GetScript();
    out2.nValue = 50 * CENT;

    BOOST_CHECK(out2.scriptPubKey.IsSidechainProposalCommit());

    // Update scdbTest to add the proposal
    scdbTest.Update(0, GetRandHash(), uint256(), std::vector<CTxOut>{out, out2});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    // Nothing should have been added
    BOOST_CHECK(vActivation.empty());
}

BOOST_AUTO_TEST_CASE(sidechainactivation_invalid)
{
    // Invalid name
    // Invalid description
    // Invalid private key
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate)
{
    // Test adding one proposal to scdbTest and activating it
    SidechainDB scdbTest;

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

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
        uint256 hash2 = GetRandHash();
        scdbTest.Update(i, hash2, hash1, block.vtx.front()->vout);
        hash1 = hash2;
    }

    // Check activation status
    // Sidechain should have been removed from activation cache
    // Sidechain should be in ValidSidechains
    vActivation = scdbTest.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 1 && vSidechain.front() == proposal);
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate_multi)
{
    // Test adding two proposal to scdbTest and activating them, in a single voting
    // period
    SidechainDB scdbTest;

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    // Create another sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal2.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal2.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal2.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    out.scriptPubKey = proposal2.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash2 = GetRandHash();
    scdbTest.Update(1, hash2, hash1, std::vector<CTxOut>{out});

    vActivation = scdbTest.GetSidechainActivationStatus();

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
        hash1 = GetRandHash();
        scdbTest.Update(i, hash1, hash2, block.vtx.front()->vout);
        hash2 = hash1;
    }

    // Check activation status
    // Sidechains should have been removed from activation cache
    // Sidechains should be in ValidSidechains
    vActivation = scdbTest.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 2 && vSidechain.front() == proposal && vSidechain.back() == proposal2);
}

BOOST_AUTO_TEST_CASE(sidechainactivation_activate_multi_seperate)
{
    // Test adding two proposal to scdbTest and activating them, in seperate voting
    // periods
    SidechainDB scdbTest;

    // Create sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

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
        uint256 hash2 = GetRandHash();
        scdbTest.Update(i, hash2, hash1, block.vtx.front()->vout);
        hash1 = hash2;
    }

    // Check activation status
    // Sidechain should have been removed from activation cache
    // Sidechain should be in ValidSidechains
    vActivation = scdbTest.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    // Proposal 1 should have activated
    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 1 && vSidechain.front() == proposal);

    // Create another sidechain proposal
    SidechainProposal proposal2;
    proposal2.title = "test2";
    proposal2.description = "description";
    proposal2.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal2.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal2.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal2.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal2.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    out.scriptPubKey = proposal2.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash3 = GetRandHash();
    scdbTest.Update(1, hash3, hash1, std::vector<CTxOut>{out});

    vActivation = scdbTest.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal2.GetHash()));

    // Generate commit for proposal 2
    block.vtx.clear();
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));
    GenerateSidechainActivationCommitment(block, proposal2.GetHash(), Params().GetConsensus());

    // Add votes until the sidechain is activated
    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_AGE; i++) {
        uint256 hash4 = GetRandHash();
        scdbTest.Update(i, hash4, hash3, block.vtx.front()->vout);
        hash3 = hash4;
    }

    // Now proposal 2 should be removed from the activation cache and should
    // be in the valid sidechain vector
    vActivation = scdbTest.GetSidechainActivationStatus();
    BOOST_CHECK(vActivation.empty());

    vSidechain = scdbTest.GetActiveSidechains();
    BOOST_CHECK(vSidechain.size() == 2 && vSidechain.front() == proposal && vSidechain.back() == proposal2);
}

BOOST_AUTO_TEST_CASE(sidechainactivation_failActivation)
{
    // Test adding one proposal to scdbTest and fail to activate it
    SidechainDB scdbTest;

    // Creat sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

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
        uint256 hash2 = GetRandHash();
        scdbTest.Update(i, hash2, hash1, block.vtx.front()->vout);
        hash1 = hash2;
    }

    // Check activation status
    // Sidechain should still be in activation cache
    // Sidechain should not be in ValidSidechains
    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();

    BOOST_CHECK(vSidechain.empty());
}

BOOST_AUTO_TEST_CASE(sidechainactivation_pruneRejected)
{
    // Test that sidechains which have no chance of success (based on their
    // rejection count) are pruned from the activation cache.
    SidechainDB scdbTest;

    // Creat sidechain proposal
    SidechainProposal proposal;
    proposal.title = "test";
    proposal.description = "description";
    proposal.sidechainKeyID = "80dca759b4ff2c9e9b65ec790703ad09fba844cd";
    proposal.sidechainHex = "76a91480dca759b4ff2c9e9b65ec790703ad09fba844cd88ac";
    proposal.sidechainPriv = "5Jf2vbdzdCccKApCrjmwL5EFc4f1cUm5Ah4L4LGimEuFyqYpa9r";
    proposal.hashID1 = uint256S("b55d224f1fda033d930c92b1b40871f209387355557dd5e0d2b5dd9bb813c33f");
    proposal.hashID2 = uint160S("31d98584f3c570961359c308619f5cf2e9178482");

    // Create transaction output with sidechain proposal
    CTxOut out;
    out.scriptPubKey = proposal.GetScript();
    out.nValue = 50 * CENT;

    BOOST_CHECK(out.scriptPubKey.IsSidechainProposalCommit());

    uint256 hash1 = GetRandHash();
    scdbTest.Update(0, hash1, uint256(), std::vector<CTxOut>{out});

    std::vector<SidechainActivationStatus> vActivation;
    vActivation = scdbTest.GetSidechainActivationStatus();

    BOOST_CHECK((vActivation.size() == 1) && (vActivation.front().proposal.GetHash() == proposal.GetHash()));

    // Pass coinbase without sidechain activation commit into scdbTest enough times
    // that the proposal is rejected and pruned.
    out.scriptPubKey = OP_TRUE;
    out.nValue = 50 * CENT;

    for (int i = 1; i <= SIDECHAIN_ACTIVATION_MAX_FAILURES + 1; i++) {
        vActivation = scdbTest.GetSidechainActivationStatus();
        uint256 hash2 = GetRandHash();
        scdbTest.Update(i, hash2, hash1, std::vector<CTxOut>{out});
        hash1 = hash2;
    }

    // Check activation status
    // Sidechain should have been pruned from activation cache
    // Sidechain should not be in ValidSidechains
    std::vector<SidechainActivationStatus> vActivationFinal;
    vActivationFinal = scdbTest.GetSidechainActivationStatus();
    BOOST_CHECK(vActivationFinal.empty());

    std::vector<Sidechain> vSidechain = scdbTest.GetActiveSidechains();
    BOOST_CHECK(vSidechain.empty());
}

BOOST_AUTO_TEST_CASE(sidechainactivation_duplicateOfActivated)
{
    // Test proposing a sidechain that is an exact duplicate of a sidechain
    // that has already activated. Should be rejected.
}

BOOST_AUTO_TEST_SUITE_END()
