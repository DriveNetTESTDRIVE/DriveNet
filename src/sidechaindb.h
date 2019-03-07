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
class COutPoint;
class CScript;
class CTransaction;
class CTxOut;
class uint256;

struct Sidechain;
struct SidechainActivationStatus;
struct SidechainCTIP;
struct SidechainDeposit;
struct SidechainLD;
struct SidechainProposal;
struct SidechainUpdateMSG;
struct SidechainUpdatePackage;
struct SidechainWTPrimeState;

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
    bool AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight, bool fDebug = false);

    /** Add active sidechains to the in-memory cache */
    void CacheActiveSidechains(const std::vector<Sidechain>& vSidechainIn);

    /** Add SidechainActivationStatus to the in-memory cache */
    void CacheSidechainActivationStatus(const std::vector<SidechainActivationStatus>& vActivationStatusIn);

    /** Add SidechainProposal to the in-memory cache */
    void CacheSidechainProposals(const std::vector<SidechainProposal>& vSidechainProposalIn);

    /** Add WT^ to the in-memory cache */
    bool CacheWTPrime(const CTransaction& tx);

    /** Add sidechain-to-be-activated hash to cache */
    void CacheSidechainHashToActivate(const uint256& u);

    /** Remove sidechain-to-be-activated hash from cache,
     * because the user changed their mind */
    void RemoveSidechainHashToActivate(const uint256& u);

    /** Count ratchet member blocks atop */
    int CountBlocksAtop(const CCriticalData& data) const;

    /** Count ratchet member blocks atop (overload) */
    int CountBlocksAtop(const SidechainLD& ld) const;

    /** Check SCDB WT^ verification status */
    bool CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime, bool fDebug = false) const;

    /** Return number of active sidechains */
    int GetActiveSidechainCount() const;

    /** Return vector of deposits this verification period for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(uint8_t nSidechain) const;

    /** Return vector of deposits this verification period for the sidechain. */
    std::vector<SidechainDeposit> GetDeposits(const std::string& sidechainPriv) const;

    /** Return serialization hash of BMM ratchet data */
    uint256 GetBMMHash() const;

    /** Return serialization hash of SCDB latest verification(s) */
    uint256 GetSCDBHash() const;

    /** Check if the hash of the sidechain is in our hashes of sidechains to
     * activate cache. Return true if it is, or false if not. */
    bool GetActivateSidechain(const uint256& u) const;

    /** Return the hash of the last block SCDB processed */
    uint256 GetHashBlockLastSeen();

    /** Return what the SCDB hash would be if the updates are applied */
    uint256 GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight) const;

    /**  Return BMM ratchet data for the specified sidechain, if valid */
    bool GetLinkingData(uint8_t nSidechain, std::vector<SidechainLD>& ld) const;

    /** Get the scriptPubKey that relates to nSidehcain if it exists */
    bool GetSidechainScript(const uint8_t nSidechain, CScript& scriptPubKey) const;

    /** Get the sidechain that relates to nSidechain if it exists */
    bool GetSidechain(const uint8_t nSidechain, Sidechain& sidechain) const;

    /** Get status of nSidechain's WT^(s) (public for unit tests) */
    std::vector<SidechainWTPrimeState> GetState(uint8_t nSidechain) const;

    /** Return the cached WT^ transaction(s) */
    std::vector<CTransaction> GetWTPrimeCache() const;

    /** Return cached but uncommitted WT^ transaction's hash(s) for nSidechain */
    std::vector<uint256> GetUncommittedWTPrimeCache(uint8_t nSidechain) const;

    /** Return cached but uncommitted WT^ transaction's hash(s) for nSidechain */
    std::vector<SidechainProposal> GetUncommittedSidechainProposals() const;

    /** Is there anything being tracked by the SCDB? */
    bool HasState() const;

    /** Return true if the deposit is cached */
    bool HaveDepositCached(const SidechainDeposit& deposit) const;

    /** Return true if LD is in the ratchet */
    bool HaveLinkingData(uint8_t nSidechain, uint256 hashCritical) const;

    /** Return true if the transcation spends sidechain inputs. Return the
     * sidechain number by reference */
    bool HasSidechainScript(const std::vector<CScript>& vScript, uint8_t& nSidechain) const;

    /** Return true if the full WT^ CTransaction is cached */
    bool HaveWTPrimeCached(const uint256& hashWTPrime) const;

    /** Check if SCDB is tracking the work score of a WT^ */
    bool HaveWTPrimeWorkScore(const uint256& hashWTPrime, uint8_t nSidechain) const;

    /** Reset SCDB and clear out all data tracked by SidechainDB */
    void ResetWTPrimeState();

    /** Reset active sidechains and sidechain activation status */
    void ResetSidechains();

    /** Print SCDB WT^ verification status */
    std::string ToString() const;

    /**
     * Update the DB state.
     */
    bool Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError, bool fDebug = false);

    /** Update / add multiple SCDB WT^(s) to SCDB */
    bool UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, bool fDebug = false);

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

    /** Get sidechain activation status */
    std::vector<SidechainActivationStatus> GetSidechainActivationStatus() const;

    /** Get list of currently active sidechains */
    std::vector<Sidechain> GetActiveSidechains() const;

    /** Get list of this node's uncomitted sidechain proposals */
    std::vector<SidechainProposal> GetSidechainProposals() const;

    std::string GetSidechainName(uint8_t nSidechain) const;

    bool IsSidechainNumberValid(uint8_t nSidechain) const;

    std::vector<uint256> GetSidechainsToActivate() const;

    bool GetCTIP(uint8_t nSidechain, SidechainCTIP& out) const;

    std::map<uint8_t, SidechainCTIP> GetCTIP() const;

private:
    /** Tracks verification status of WT^(s) */
    std::vector<std::vector<SidechainWTPrimeState>> vWTPrimeStatus;

    /** Sidechains which are currently active */
    std::vector<Sidechain> vActiveSidechain;

    /** Activation status of proposed sidechains */
    std::vector<SidechainActivationStatus> vActivationStatus;

    /** Cache of sidechain hashes, for sidechains which this node has
     * been configured to activate by the user*/
    std::vector<uint256> vSidechainHashActivate;

    /** Cache of proposals created by this node, which should be included in the
     * next block that this node mines. *
     */
    std::vector<SidechainProposal> vSidechainProposal;

    /** BMM ratchet */
    std::vector<std::vector<SidechainLD>> ratchet;

    /** Cache of potential WT^ transactions */
    std::vector<CTransaction> vWTPrimeCache;

    /** Cache of deposits created during this verification period */
    std::vector<SidechainDeposit> vDepositCache;

    // TODO remove
    /** Cache of WT^ update messages. */
    std::vector<SidechainUpdatePackage> vSidechainUpdateCache;

    /*
     * The CTIP of nSidechain up to the latest connected block (does not
     * include mempool txns).
     */
    std::map<uint8_t, SidechainCTIP> mapCTIP;

    /** The most recent block that SCDB has processed */
    uint256 hashBlockLastSeen;

    /**
     * Submit default vote for all sidechain WT^(s).
     * Used when a new block does not contain a valid update.
     */
    bool ApplyDefaultUpdate();

    /* Takes a list of sidechain hashes to upvote */
    void UpdateActivationStatus(const std::vector<uint256>& vHash);

    /* Takes a list of sidechain hashes to undo upvotes */
    void UndoActivationStatusUpdate(const std::vector<uint256>& vHash);
};

/** Return height at which the current WT^ verification period began */
int GetLastSidechainVerificationPeriod(int nHeight);

/** Return the number of blocks that have been mined in this period so far */
int GetNumBlocksSinceLastSidechainVerificationPeriod(int nHeight);

#endif // BITCOIN_SIDECHAINDB_H

