// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "validation.h"
#include "net.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <algorithm>
#include <queue>
#include <utility>

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

static const bool fMiningReqiresPeer = false;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockWeight = 0;

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max<size_t>(4000, std::min<size_t>(MAX_BLOCK_WEIGHT - 4000, options.nBlockMaxWeight));
}

static BlockAssembler::Options DefaultOptions(const CChainParams& params)
{
    // Block resource limits
    // If neither -blockmaxsize or -blockmaxweight is given, limit to DEFAULT_BLOCK_MAX_*
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    BlockAssembler::Options options;
    options.nBlockMaxWeight = gArgs.GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
    if (gArgs.IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions(params)) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;

    bool fDrivechainEnabled = IsDrivechainEnabled(pindexPrev, chainparams.GetConsensus());

    if (fDrivechainEnabled) {
        // Removed expired BMM requests that we don't want to consider
        mempool.RemoveExpiredCriticalRequests();

        // Select which BMM requests (if any) to include
        mempool.SelectBMMRequests();
    }

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    bool fNeedCriticalFeeTx = false;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated, fDrivechainEnabled, fNeedCriticalFeeTx);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;

    // Coinbase subsidy + fees
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;

    // Add coinbase to block
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));

    std::vector<Sidechain> vActiveSidechain;
    if (fDrivechainEnabled)
        vActiveSidechain = scdb.GetActiveSidechains();

    if (fDrivechainEnabled) {
        // Add WT^(s) which have been validated
        for (const Sidechain& s : vActiveSidechain) {
            CMutableTransaction wtx;
            bool fCreated = CreateWTPrimePayout(s.nSidechain, wtx);
            if (fCreated && wtx.vout.size() && wtx.vin.size()) {
                pblock->vtx.push_back(MakeTransactionRef(std::move(wtx)));
            }
        }
    }

    if (fDrivechainEnabled) {
        if (scdb.HasState()) {
            bool fPeriodEnded = (nHeight % SIDECHAIN_VERIFICATION_PERIOD == 0);
            uint256 hashSCDB;
            if (!fPeriodEnded) {
                // Check if the user has set a default WT^ vote
                std::string strDefaultVote = "";
                strDefaultVote = gArgs.GetArg("-defaultwtprimevote", "");
                if (strDefaultVote == "upvote") {
                    hashSCDB = scdb.GetSCDBHashIfUpdate(scdb.GetVotes(SCDB_UPVOTE), nHeight);
                }
                else
                if (strDefaultVote == "downvote") {
                    hashSCDB = scdb.GetSCDBHashIfUpdate(scdb.GetVotes(SCDB_DOWNVOTE), nHeight);
                }
                else {
                    hashSCDB = scdb.GetSCDBHashIfUpdate(scdb.GetVotes(SCDB_ABSTAIN), nHeight);
                }

                // Check if the user has set any custom WT^ votes. They can set
                // custom upvotes and custom downvotes by specifying the WT^
                // hash as a command line param. Note that there can be multiple
                // custom votes of each type and that's why we use GetArgs()
                std::vector<std::string> vHashUpvote = gArgs.GetArgs("-upvote");
                std::vector<std::string> vHashDownvote = gArgs.GetArgs("-downvote");
                // TODO use custom WT^ votes based on WT^ hash

                // TODO
                // If params are not set, check for GUI configuration
            }
            if ((!fPeriodEnded && !hashSCDB.IsNull()) || fPeriodEnded)
                GenerateSCDBHashMerkleRootCommitment(*pblock, hashSCDB, chainparams.GetConsensus());
        }
        GenerateCriticalHashCommitments(*pblock, chainparams.GetConsensus());

        // TODO make interactive - GUI
        // Commit WT^(s) which we have received locally
        for (const Sidechain& s : vActiveSidechain) {
            std::vector<uint256> vFreshWTPrime;
            vFreshWTPrime = scdb.GetUncommittedWTPrimeCache(s.nSidechain);

            if (vFreshWTPrime.empty())
                continue;

            // For now, if there are fresh (uncommited, unknown to SCDB) WT^(s)
            // we will commit the most recent in the block we are generating.
            GenerateWTPrimeHashCommitment(*pblock, vFreshWTPrime.back(), s.nSidechain, chainparams.GetConsensus());
        }

        // TODO this should loop through sidechains with activation status,
        // and activated sidechains to figure out which proposals we haven't
        // proposed yet.
        // Commit the oldest uncommitted sidechain proposal that we have created
        //
        // If we commit a proposal, save the hash to easily ACK it later
        uint256 hashProposal;
        std::vector<SidechainProposal> vProposal = scdb.GetSidechainProposals();
        if (!vProposal.empty()) {
            GenerateSidechainProposalCommitment(*pblock, vProposal.front(), chainparams.GetConsensus());
            hashProposal = vProposal.front().GetHash();
        }

        // TODO for now, if this is set to 1 (true), activate any sidechain
        // which has been proposed. Make this behavior the default unless a
        // list of sha256 hashes of sidechains is also provided to the command
        // line, in which case only activate those sidechain(s).
        bool fAnySidechain = gArgs.GetBoolArg("-activatesidechains", false);

        // Commit sidechain activation for proposals in activation status cache
        // which we have configured to ACK
        std::vector<SidechainActivationStatus> vActivationStatus;
        vActivationStatus = scdb.GetSidechainActivationStatus();
        for (const SidechainActivationStatus& s : vActivationStatus) {
            if (fAnySidechain || scdb.GetActivateSidechain(s.proposal.GetHash()))
                GenerateSidechainActivationCommitment(*pblock, s.proposal.GetHash(), chainparams.GetConsensus());
        }
        // If we've proposed a sidechain in this block, ACK it
        if (!hashProposal.IsNull()) {
            GenerateSidechainActivationCommitment(*pblock, hashProposal, chainparams.GetConsensus());
        }
    }

    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    // Handle / create critical fee tx (collects bmm / critical data fees)
    if (fDrivechainEnabled && fNeedCriticalFeeTx) {
        // Create critical fee tx
        CMutableTransaction feeTx;
        feeTx.vout.resize(1);
        // Pay the fees to the same script as the coinbase
        feeTx.vout[0].scriptPubKey = scriptPubKeyIn;
        feeTx.vout[0].nValue = CAmount(0);

        // Find all of the critical data transactions included in the block
        // and take their input and total amount
        for (const CTransactionRef& tx : pblock->vtx) {
            if (tx && !tx->criticalData.IsNull()) {
                // Try to find the critical data fee output and take it
                for (uint32_t i = 0; i < tx->vout.size(); i++) {
                    if (tx->vout[i].scriptPubKey == CScript() << OP_TRUE) {
                        feeTx.vin.push_back(CTxIn(tx->GetHash(), i));
                        feeTx.vout[0].nValue += tx->vout[i].nValue;
                    }
                }
            }
        }

        // Add the fee tx to the block
        if (CTransaction(feeTx).GetValueOut()) {
            pblock->vtx.push_back(MakeTransactionRef(std::move(feeTx)));
            pblocktemplate->vTxSigOpsCost.push_back(WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx.back()));
            pblocktemplate->vTxFees.push_back(0);

            // Remove the old witness commitment
            int wi = GetWitnessCommitmentIndex(*pblock);
            if (wi != -1) {
                CMutableTransaction mtx(*pblock->vtx[0]);
                mtx.vout.erase(mtx.vout.begin() + wi);
                pblock->vtx[0] = MakeTransactionRef(std::move(mtx));
            }

            pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());

            // Test block validity after adding critical fee tx
            CValidationState state;
            if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, true, true)) {
                // TODO right now if the block is too big or invalid after this
                // will result in giving up the BMM commitment fees...

                // Remove fee tx if the block is too big or otherwise invalid
                pblock->vtx.pop_back();
                pblocktemplate->vTxSigOpsCost.pop_back();
                pblocktemplate->vTxFees.pop_back();

                // Remove the old witness commitment
                int wi = GetWitnessCommitmentIndex(*pblock);
                if (wi != -1) {
                    CMutableTransaction mtx(*pblock->vtx[0]);
                    mtx.vout.erase(mtx.vout.begin() + wi);
                    pblock->vtx[0] = MakeTransactionRef(std::move(mtx));
                }

                pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
            }
        }
    }

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), nPackagesSelected, nDescendantsUpdated, 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
// - critical data request height
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
        if (!it->GetTx().criticalData.IsNull()) {
            if (nHeight != (int64_t)it->GetTx().nLockTime + 1)
                return false;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

bool BlockAssembler::CreateWTPrimePayout(uint8_t nSidechain, CMutableTransaction& tx)
{
    // TODO log all false returns

    // The WT^ that will be created
    CMutableTransaction mtx;
    mtx.nVersion = 2;

    if (!IsDrivechainEnabled(chainActive.Tip(), chainparams.GetConsensus()))
        return false;

#ifdef ENABLE_WALLET
    if (!scdb.HasState())
        return false;
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    Sidechain sidechain;
    if (!scdb.GetSidechain(nSidechain, sidechain))
        return false;

    // Select the highest scoring B-WT^ for sidechain during verification period
    uint256 hashBest = uint256();
    uint16_t scoreBest = 0;
    std::vector<SidechainWTPrimeState> vState = scdb.GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.nWorkScore > scoreBest || scoreBest == 0) {
            hashBest = state.hashWTPrime;
            scoreBest = state.nWorkScore;
        }
    }
    if (hashBest == uint256())
        return false;

    // Does the selected B-WT^ have sufficient work score?
    if (scoreBest < SIDECHAIN_MIN_WORKSCORE)
        return false;

    // Copy outputs from B-WT^
    std::vector<CTransaction> vWTPrime = scdb.GetWTPrimeCache();
    for (const CTransaction& tx : vWTPrime) {
        if (tx.GetHash() == hashBest) {
            for (const CTxOut& out : tx.vout)
                mtx.vout.push_back(out);
            break;
        }
    }
    if (!mtx.vout.size())
        return false;

    // Calculate the amount to be withdrawn by WT^
    CAmount amtBWT = CAmount(0);
    for (const CTxOut& out : mtx.vout) {
        const CScript scriptPubKey = out.scriptPubKey;
        if (HexStr(scriptPubKey) != sidechain.sidechainHex) {
            amtBWT += out.nValue;
        }
    }

    // Format sidechain change return script
    CScript sidechainScript;
    if (!scdb.GetSidechainScript(nSidechain, sidechainScript))
        return false;

    // Add placeholder change return as last output
    mtx.vout.push_back(CTxOut(0, sidechainScript));

    // Get sidechain's CTIP
    SidechainCTIP ctip;
    if (!scdb.GetCTIP(nSidechain, ctip))
        return false;

    mtx.vin.push_back(CTxIn(ctip.out));

    // Start calculating amount returning to sidechain
    CAmount returnAmount = ctip.amount;
    mtx.vout.back().nValue += returnAmount;

    // Subtract payout amount from sidechain change return
    mtx.vout.back().nValue -= amtBWT;

    if (mtx.vout.back().nValue < 0)
        return false;
    if (!mtx.vin.size())
        return false;

    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(sidechain.sidechainPriv);
    if (!fGood)
        return false;

    CKey privKey = vchSecret.GetKey();
    if (!privKey.IsValid())
        return false;

    // Set up keystore with sidechain's private key
    CBasicKeyStore tempKeystore;
    tempKeystore.AddKey(privKey);
    const CKeyStore& keystoreConst = tempKeystore;

    // Sign WT^ SCUTXO input
    const CTransaction& txToSign = mtx;
    TransactionSignatureCreator creator(&keystoreConst, &txToSign, 0, returnAmount - amtBWT);
    SignatureData sigdata;
    bool sigCreated = ProduceSignature(creator, sidechainScript, sigdata);
    if (!sigCreated)
        return false;

    mtx.vin[0].scriptSig = sigdata.scriptSig;
#endif

    // Check to make sure that all of the outputs in this WT^ are unknown / new
    for (size_t o = 0; o < mtx.vout.size(); o++) {
        if (pcoinsTip->HaveCoin(COutPoint(mtx.GetHash(), o))) {
            return false;
        }
    }

    tx = mtx;

    return true;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated, bool fDrivechainEnabled, bool& fNeedCriticalFeeTx)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);

            // Set fNeedCriticalFeeTx
            if (fDrivechainEnabled && sortedEntries[i]->HasCriticalData())
                fNeedCriticalFeeTx = true;
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
bool static ScanHash(const CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
{
    // Write the first 76 bytes of the block header to a double-SHA256 state.
    SHAndwich256 hasher;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *pblock;
    assert(ss.size() == 80);
    hasher.Write((unsigned char*)&ss[0], 76);

    while (true) {
        nNonce++;

        // Write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-SHA256 state, and compute the result.
        SHAndwich256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((uint16_t*)phash)[15] == 0)
            return true;

        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xfff) == 0)
            return false;
    }
}

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0]->vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    std::shared_ptr<const CBlock> block = std::make_shared<CBlock>(*pblock);
    CValidationState state;
    if (!ProcessNewBlock(Params(), block, true, nullptr))
        return error("BitcoinMiner: ProcessNewBlock, block not accepted");

    return true;
}

void static BitcoinMiner(const CChainParams& chainparams)
{
    LogPrintf("BitcoinMiner started\n");
    //SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("drivenet-miner");

    unsigned int nExtraNonce = 0;

    if (vpwallets.empty())
        return; // TODO error message

    std::shared_ptr<CReserveScript> coinbaseScript;
    vpwallets[0]->GetScriptForMining(coinbaseScript);

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            if (fMiningReqiresPeer) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                // TODO
                /*
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
                */
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            int nMinerSleep = gArgs.GetArg("-minersleep", 0);
            if (nMinerSleep)
                MilliSleep(nMinerSleep);

            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript));
            if (!pblocktemplate.get())
            {
                LogPrintf("Error in BitcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running BitcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            while (true) {
                // Check if something found
                if (ScanHash(pblock, nNonce, &hash))
                {
                    if (UintToArith256(hash) <= hashTarget)
                    {
                        // Found a solution
                        pblock->nNonce = nNonce;
                        assert(hash == pblock->GetPoWHash());

                        LogPrintf("BitcoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, chainparams);
                        coinbaseScript->KeepScript();

                        break;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                // TODO
                /*
                if (vNodes.empty() && fMiningRequiresPeer)
                    break;
                */
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    break; // Recreate the block if the clock has run backwards,
                           // so that we can use the correct time.
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("BitcoinMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("BitcoinMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, boost::cref(chainparams)));
}
