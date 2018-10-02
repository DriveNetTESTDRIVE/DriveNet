// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include <map>
#include <queue>
#include <vector>

#include <uint256.h>

class CCriticalData;
class CScript;
class CTransaction;
class CTxOut;
class uint256;

struct Sidechain;
struct SidechainDeposit;
struct SidechainLD;
struct SidechainUpdateMSG;
struct SidechainUpdatePackage;
struct SidechainWTPrimeState;
struct SCDBIndex;

class SidechainDB
{
public:
    SidechainDB();

    /** Add deposit(s) to cache */
    void AddDeposits(const std::vector<CTransaction>& vtx, const uint256& hashBlock);

    void AddDeposits(const std::vector<SidechainDeposit>& vDeposit);

    /** Cache WT^ update TODO here for testing, move to networking */
    void AddSidechainNetworkUpdatePackage(const SidechainUpdatePackage& update);

    /** Add a new WT^ to the database */
    bool AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight);

    bool CacheWTPrime(uint8_t nSidechain, const CTransaction& tx);

    /** Count ratchet member blocks atop */
    int CountBlocksAtop(const CCriticalData& data) const;

    /** Count ratchet member blocks atop (overload) */
    int CountBlocksAtop(const SidechainLD& ld) const;

    /** Check SCDB WT^ verification status */
    bool CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime) const;

    /** Return vector of deposits this verification period for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(uint8_t nSidechain) const;

    /** Return serialization hash of BMM ratchet data */
    uint256 GetBMMHash() const;

    /** Return serialization hash of SCDB latest verification(s) */
    uint256 GetSCDBHash() const;

    /** Return the hash of the last block SCDB processed */
    uint256 GetHashBlockLastSeen();

    /** Return what the SCDB hash would be if the updates are applied */
    uint256 GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight) const;

    /**  Return BMM ratchet data for the specified sidechain, if valid */
    bool GetLinkingData(uint8_t nSidechain, std::vector<SidechainLD>& ld) const;

    /** Get status of nSidechain's WT^(s) (public for unit tests) */
    std::vector<SidechainWTPrimeState> GetState(uint8_t nSidechain) const;

    /** Return the cached WT^ transaction(s) */
    std::vector<CTransaction> GetWTPrimeCache() const;

    /** Return cached but uncommitted WT^ transaction's hash(s) for nSidechain */
    std::vector<uint256> GetUncommittedWTPrimeCache(uint8_t nSidechain) const;

    /** Is there anything being tracked by the SCDB? */
    bool HasState() const;

    /** Return true if the deposit is cached */
    bool HaveDepositCached(const SidechainDeposit& deposit) const;

    /** Return true if LD is in the ratchet */
    bool HaveLinkingData(uint8_t nSidechain, uint256 hashCritical) const;

    /** Return true if the full WT^ CTransaction is cached */
    bool HaveWTPrimeCached(const uint256& hashWTPrime) const;

    /** Check if SCDB is tracking the work score of a WT^ */
    bool HaveWTPrimeWorkScore(const uint256& hashWTPrime, uint8_t nSidechain) const;

    /** Reset SCDB and clear out all data tracked by SidechainDB */
    void Reset();

    /** Print SCDB WT^ verification status */
    std::string ToString() const;

    /**
     * Update the DB state.
     */
    bool Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError);

    /** Update / add multiple SCDB WT^(s) to SCDB */
    bool UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight);

    /** Read the SCDB hash in a new block and try to synchronize our SCDB
     *  by testing possible work score updates until the SCDB hash of our
     *  SCDB matches that of the new block. Return false if no match found.
     */
    bool UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot);

    /** Get state with upvotes applied to all WT^(s) */
    std::vector<SidechainWTPrimeState> GetDownvotes() const;

    /** Get state with abstain votes applied to all WT^(s) */
    std::vector<SidechainWTPrimeState> GetAbstainVotes() const;

    /** Get state with downvotes applied to all WT^(s) */
    std::vector<SidechainWTPrimeState> GetUpvotes() const;

private:
    /** Sidechain "database" tracks verification status of WT^(s) */
    std::vector<SCDBIndex> SCDB;

    /** BMM ratchet */
    std::vector<std::vector<SidechainLD>> ratchet;

    /** Cache of potential WT^ transactions */
    std::vector<CTransaction> vWTPrimeCache;

    /** Cache of deposits created during this verification period */
    std::vector<SidechainDeposit> vDepositCache;

    /** Cache of WT^ update messages.
    *  TODO This is here to enable testing, remove
    *  when RPC calls are replaced with network messages.
    */
    std::vector<SidechainUpdatePackage> vSidechainUpdateCache;

    /** The most recent block that SCDB has processed */
    uint256 hashBlockLastSeen;

    /**
     * Submit default vote for all sidechain WT^(s).
     * Used when a new block does not contain a valid update.
     */
    bool ApplyDefaultUpdate();

};

/** Return height at which the current WT^ verification period began */
int GetLastSidechainVerificationPeriod(int nHeight);

/** Return the number of blocks that have been mined in this period so far */
int GetNumBlocksSinceLastSidechainVerificationPeriod(int nHeight);

#endif // BITCOIN_SIDECHAINDB_H

