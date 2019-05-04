// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidechaindb.h>

#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <sidechain.h>
#include <uint256.h>
#include <util.h> // For LogPrintf TODO move LogPrintf
#include <utilstrencodings.h>

SidechainDB::SidechainDB()
{
}

void SidechainDB::AddDeposits(const std::vector<CTransaction>& vtx, const uint256& hashBlock)
{
    std::vector<SidechainDeposit> vDeposit;
    for (const CTransaction& tx : vtx) {
        // Create sidechain deposit objects from transaction outputs
        SidechainDeposit deposit;
        bool fBurnFound = false;
        for (size_t i = 0; i < tx.vout.size(); i++) {
            const CScript &scriptPubKey = tx.vout[i].scriptPubKey;
            uint8_t nSidechainTmp;
            if (HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechainTmp)) {
                // Copy output index of deposit burn and move on
                deposit.n = i;
                fBurnFound = true;
                continue;
            }

            // scriptPubKey must contain keyID
            if (scriptPubKey.size() != 23 && scriptPubKey.size() != 24)
                continue;
            if (scriptPubKey.front() != OP_RETURN)
                continue;

            uint8_t nSidechain = (unsigned int)scriptPubKey[1];
            if (!IsSidechainNumberValid(nSidechain))
                continue;

            CScript::const_iterator pkey = scriptPubKey.begin() + 2 + (scriptPubKey.size() == 24);
            opcodetype opcode;
            std::vector<unsigned char> vch;
            if (!scriptPubKey.GetOp(pkey, opcode, vch))
                continue;
            if (vch.size() != sizeof(uint160))
                continue;

            CKeyID keyID = CKeyID(uint160(vch));
            if (keyID.IsNull())
                continue;

            deposit.tx = tx;
            deposit.keyID = keyID;
            deposit.nSidechain = nSidechain;
            deposit.hashBlock = hashBlock;
        }
        // TODO Confirm that deposit.nSidechain is correct by comparing deposit
        // output KeyID with sidechain KeyID before adding deposit to cache.
        // TODO Confirm single burn
        if (CTransaction(deposit.tx) == tx && fBurnFound) {
            vDeposit.push_back(deposit);
        }
    }

    // Add deposits to cache
    AddDeposits(vDeposit);
}

void SidechainDB::AddDeposits(const std::vector<SidechainDeposit>& vDeposit)
{
    for (const SidechainDeposit& d : vDeposit) {
        if (!IsSidechainNumberValid(d.nSidechain))
            continue;
        if (HaveDepositCached(d))
            continue;

        COutPoint out(d.tx.GetHash(), d.n);
        CAmount amount = d.tx.vout[d.n].nValue;

        SidechainCTIP ctip;
        ctip.out = out;
        ctip.amount = amount;

        mapCTIP[d.nSidechain] = ctip;
        vDepositCache.push_back(d);
    }
}

// TODO remove
void SidechainDB::AddSidechainNetworkUpdatePackage(const SidechainUpdatePackage& update)
{
    vSidechainUpdateCache.push_back(update);
}

bool SidechainDB::AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight, bool fDebug)
{
    if (!IsSidechainNumberValid(nSidechain)) {
        LogPrintf("%s: Rejected WT^: %s. Invalid sidechain number: %u\n",
                __func__,
                hashWTPrime.ToString());
        return false;
    }

    if (HaveWTPrimeWorkScore(hashWTPrime, nSidechain)) {
        LogPrintf("%s: Rejected WT^: %s already known\n",
                __func__,
                hashWTPrime.ToString());
        return false;
    }

    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.nSidechain = nSidechain;

    int nAge = GetNumBlocksSinceLastSidechainVerificationPeriod(nHeight);
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nAge;
    wt.nWorkScore = 1;
    wt.hashWTPrime = hashWTPrime;

    vWT.push_back(wt);

    if (fDebug)
        LogPrintf("%s: Cached WT^: %s\n", __func__, hashWTPrime.ToString());

    bool fUpdated = UpdateSCDBIndex(vWT, nHeight, true /* fDebug */);

    if (!fUpdated && fDebug)
        LogPrintf("%s: Failed to update SCDBIndex.\n", __func__);

    return fUpdated;
}

void SidechainDB::CacheActiveSidechains(const std::vector<Sidechain>& vActiveSidechainIn)
{
    vActiveSidechain = vActiveSidechainIn;

    // Also resize vWTPrimeStatus to keep track of WT^(s)
    vWTPrimeStatus.resize(vActiveSidechain.size());
}

void SidechainDB::CacheSidechainActivationStatus(const std::vector<SidechainActivationStatus>& vActivationStatusIn)
{
    vActivationStatus = vActivationStatusIn;
}

void SidechainDB::CacheSidechainProposals(const std::vector<SidechainProposal>& vSidechainProposalIn)
{
    for (const SidechainProposal& s : vSidechainProposalIn)
        vSidechainProposal.push_back(s);
}

void SidechainDB::CacheSidechainHashToActivate(const uint256& u)
{
    vSidechainHashActivate.push_back(u);
}

bool SidechainDB::CacheWTPrime(const CTransaction& tx)
{
    if (vActiveSidechain.empty())
        return false;
    if (HaveWTPrimeCached(tx.GetHash()))
        return false;

    vWTPrimeCache.push_back(tx);

    return true;
}

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime, bool fDebug) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime) {
            if (state.nWorkScore >= SIDECHAIN_MIN_WORKSCORE) {
                if (fDebug)
                    LogPrintf("%s: Approved: %s\n",
                            __func__,
                            hashWTPrime.ToString());
                return true;
            } else {
                if (fDebug)
                    LogPrintf("%s: Rejected: %s (insufficient work score)\n",
                            __func__,
                            hashWTPrime.ToString());
                return false;
            }
        }
    }
    if (fDebug)
        LogPrintf("%s: Rejected (WT^ state not found): %s\n",
                __func__,
                hashWTPrime.ToString());
    return false;
}

int SidechainDB::GetActiveSidechainCount() const
{
    return vActiveSidechain.size();
}

bool SidechainDB::GetActivateSidechain(const uint256& u) const
{
    // TODO change the container to make this more efficient
    for (const uint256& hash : vSidechainHashActivate) {
        if (u == hash) {
            return true;
        }
    }
    // Also check if we created the sidechain proposal, and ACK it
    for (const SidechainProposal& s : vSidechainProposal) {
        if (s.GetHash() == u) {
            return true;
        }
    }
    return false;
}

std::vector<Sidechain> SidechainDB::GetActiveSidechains() const
{
    return vActiveSidechain;
}

bool SidechainDB::GetCTIP(uint8_t nSidechain, SidechainCTIP& out) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::map<uint8_t, SidechainCTIP>::const_iterator it = mapCTIP.find(nSidechain);
    if (it != mapCTIP.end()) {
        out = it->second;
        return true;
    }

    return false;
}

std::map<uint8_t, SidechainCTIP> SidechainDB::GetCTIP() const
{
    return mapCTIP;
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(uint8_t nSidechain) const
{
    std::vector<SidechainDeposit> vDeposit;
    for (const SidechainDeposit& d : vDepositCache) {
        if (d.nSidechain == nSidechain)
            vDeposit.push_back(d);
    }
    return vDeposit;
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(const std::string& sidechainPriv) const
{
    // TODO refactor: only one GetDeposits function in SCDB

    // Make sure that the hash is related to an active sidechain,
    // and then return the result of the old function call.
    uint8_t nSidechain = 0;
    bool fFound = false;
    for (const Sidechain& s : vActiveSidechain) {
        if (s.sidechainPriv == sidechainPriv) {
            fFound = true;
            break;
        }
        nSidechain++;
    }

    if (!fFound)
        return std::vector<SidechainDeposit> {};

    return GetDeposits(nSidechain);
}

uint256 SidechainDB::GetHashBlockLastSeen()
{
    return hashBlockLastSeen;
}

uint256 SidechainDB::GetSCDBHash() const
{
    std::vector<uint256> vLeaf;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        for (const SidechainWTPrimeState& state : vState) {
            vLeaf.push_back(state.GetHash());
        }
    }
    return ComputeMerkleRoot(vLeaf);
}

uint256 SidechainDB::GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight) const
{
    SidechainDB scdbCopy = (*this);
    scdbCopy.UpdateSCDBIndex(vNewScores, nHeight);

    return (scdbCopy.GetSCDBHash());
}

bool SidechainDB::GetSidechain(const uint8_t nSidechain, Sidechain& sidechain) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    for (const Sidechain& s : vActiveSidechain) {
        if (s.nSidechain == nSidechain) {
            sidechain = s;
            return true;
        }
    }
    return false;
}

std::vector<SidechainActivationStatus> SidechainDB::GetSidechainActivationStatus() const
{
    return vActivationStatus;
}

std::string SidechainDB::GetSidechainName(uint8_t nSidechain) const
{
    std::string str = "UnknownSidechain";

    Sidechain sidechain;
    if (GetSidechain(nSidechain, sidechain))
        return sidechain.title;

    return str;
}

std::vector<SidechainProposal> SidechainDB::GetSidechainProposals() const
{
    return vSidechainProposal;
}

bool SidechainDB::GetSidechainScript(const uint8_t nSidechain, CScript& scriptPubKey) const
{
    Sidechain sidechain;
    if (!GetSidechain(nSidechain, sidechain))
        return false;

    std::vector<unsigned char> vch(ParseHex(sidechain.sidechainHex));
    scriptPubKey = CScript(vch.begin(), vch.end());

    return true;
}

std::vector<uint256> SidechainDB::GetSidechainsToActivate() const
{
    return vSidechainHashActivate;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !IsSidechainNumberValid(nSidechain))
        return std::vector<SidechainWTPrimeState>();

    std::vector<SidechainWTPrimeState> vState;
    for (auto i : vWTPrimeStatus) {
        for (const SidechainWTPrimeState& s : i) {
            vState.push_back(s);
        }
    }

    return vState;
}

std::vector<uint256> SidechainDB::GetUncommittedWTPrimeCache(uint8_t nSidechain) const
{
    std::vector<uint256> vHash;
    // TODO update the container of WT^ cache, and only loop through the
    // correct sidechain's (based on nSidechain) WT^(s).
    for (const CTransaction& t : vWTPrimeCache) {
        uint256 txid = t.GetHash();
        if (!HaveWTPrimeWorkScore(txid, nSidechain)) {
            vHash.push_back(t.GetHash());
        }
    }
    return vHash;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetVotes(VoteType vote) const
{
    std::vector<SidechainWTPrimeState> vNew;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vOld = GetState(s.nSidechain);

        if (!vOld.size())
            continue;

        SidechainWTPrimeState latest = vOld.back();
        latest.nBlocksLeft--;

        if (vote == SCDB_UPVOTE)
            latest.nWorkScore++;
        else
        if (vote == SCDB_DOWNVOTE)
            latest.nWorkScore--;

        vNew.push_back(latest);
    }
    return vNew;

}

std::vector<CTransaction> SidechainDB::GetWTPrimeCache() const
{
    return vWTPrimeCache;
}

bool SidechainDB::HasState() const
{
    // Make sure that SCDB is actually initialized
    if (vWTPrimeStatus.empty() || !GetActiveSidechainCount())
        return false;

    // Check if we have WT^ state
    for (auto i : vWTPrimeStatus) {
        if (!i.empty())
            return true;
    }

    if (vWTPrimeCache.size())
        return true;

    return false;
}

bool SidechainDB::HasSidechainScript(const std::vector<CScript>& vScript, uint8_t& nSidechain) const
{
    // Check if scriptPubKey is the deposit script of any active sidechains
    for (const CScript& scriptPubKey : vScript) {
        for (const Sidechain& s : vActiveSidechain) {
            if (HexStr(scriptPubKey) == s.sidechainHex) {
                nSidechain = s.nSidechain;
                return true;
            }
        }
    }
    return false;
}

bool SidechainDB::HaveDepositCached(const SidechainDeposit &deposit) const
{
    for (const SidechainDeposit& d : vDepositCache) {
        if (d == deposit)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTPrimeCached(const uint256& hashWTPrime) const
{
    for (const CTransaction& tx : vWTPrimeCache) {
        if (tx.GetHash() == hashWTPrime)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTPrimeWorkScore(const uint256& hashWTPrime, uint8_t nSidechain) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime)
            return true;
    }
    return false;
}

bool SidechainDB::IsSidechainNumberValid(uint8_t nSidechain) const
{
    if (nSidechain >= vActiveSidechain.size())
        return false;

    for (const Sidechain& s : vActiveSidechain) {
        if (s.nSidechain == nSidechain)
            return true;
    }

    return false;
}

void SidechainDB::RemoveSidechainHashToActivate(const uint256& u)
{
    // TODO change container to make this efficient
    for (size_t i = 0; i < vSidechainHashActivate.size(); i++) {
        if (vSidechainHashActivate[i] == u) {
            vSidechainHashActivate[i] = vSidechainHashActivate.back();
            vSidechainHashActivate.pop_back();
        }
    }
}

void SidechainDB::ResetWTPrimeState()
{
    // Clear out WT^ state
    vWTPrimeStatus.clear();
    vWTPrimeStatus.resize(vActiveSidechain.size());
}

void SidechainDB::ResetSidechains()
{
    // Clear out active sidechains
    vActiveSidechain.clear();

    // Clear out sidechain activation status
    vActivationStatus.clear();

    // Clear out list of sidechain (hashes) we want to ACK
    vSidechainHashActivate.clear();

    // Clear out our cache of sidechain proposals
    vSidechainProposal.clear();

    // Clear out our cache of sidechain deposits
    vDepositCache.clear();

    // Clear out WT^ state
    vWTPrimeStatus.clear();
}

bool SidechainDB::SpendWTPrime(uint8_t nSidechain, const uint256& hashBlock, const CTransaction& tx, bool fJustCheck, bool fDebug)
{
    if (!IsSidechainNumberValid(nSidechain)) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^ (txid): %s for sidechain number: %u.\n Invalid sidechain number.\n",
                    __func__,
                    tx.GetHash().ToString(),
                    nSidechain);
        }
        return false;
    }

    uint256 hashBlind;
    if (!tx.GetBWTHash(hashBlind)) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^ (txid): %s for sidechain number: %u.\n Cannot get blind hash.\n",
                __func__,
                tx.GetHash().ToString(),
                nSidechain);
        }
        return false;
    }

    if (!CheckWorkScore(nSidechain, hashBlind, fDebug)) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^: %s for sidechain number: %u. CheckWorkScore() failed.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    // Find the required single output returning to the sidechain script
    bool fBurnFound = false;
    uint32_t n = 0;
    uint8_t nSidechainScript;
    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CScript &scriptPubKey = tx.vout[i].scriptPubKey;
        if (HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechainScript)) {
            if (fBurnFound) {
                // We already found a sidechain script output. This second
                // sidechain output makes the WT^ invalid.
                if (fDebug) {
                    LogPrintf("%s: Cannot spend WT^: %s for sidechain number: %u. Multiple sidechain return outputs in WT^.\n",
                        __func__,
                        hashBlind.ToString(),
                        nSidechain);
                }
                return false;
            }

            // Copy output index of deposit burn and move on
            n = i;
            fBurnFound = true;
            continue;
        }
    }

    // Make sure that the sidechain output was found
    if (!fBurnFound) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^: %s for sidechain number: %u. No sidechain return output in WT^.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    // Make sure that the sidechain output is to the correct sidechain
    if (nSidechainScript != nSidechain) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^: %s for sidechain number: %u. Return output to incorrect nSidechain: %u in WT^.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain,
                nSidechainScript);
        }
        return false;
    }

    if (nSidechain >= vWTPrimeStatus.size()) {
        if (fDebug) {
            LogPrintf("%s: Cannot spend WT^: %s for sidechain number: %u. WT^ status for sidechain not found.\n",
                __func__,
                hashBlind.ToString(),
                nSidechain);
        }
        return false;
    }

    if (fJustCheck)
        return true;

    // Update CTIP
    COutPoint out(tx.GetHash(), n);
    CAmount amount = tx.vout[n].nValue;

    SidechainCTIP ctip;
    ctip.out = out;
    ctip.amount = amount;

    mapCTIP[nSidechain] = ctip;

    // Create a sidechain deposit object for the return amount
    SidechainDeposit deposit;
    deposit.nSidechain = nSidechain;
    deposit.keyID = CKeyID(uint160(ParseHex("1111111111111111111111111111111111111111")));
    deposit.tx = tx;
    deposit.n = n;
    deposit.hashBlock = hashBlock;

    AddDeposits(std::vector<SidechainDeposit>{deposit});

    // Remove WT^ work score now that is has been paid out
    vWTPrimeStatus[nSidechain].clear();

    LogPrintf("%s: Updated sidechain CTIP for nSidechain: %u. CTIP output: %s CTIP amount: %i hashBlock: %s.\n",
        __func__,
        nSidechain,
        out.ToString(),
        amount,
        hashBlock.ToString());

    return true;
}

std::string SidechainDB::ToString() const
{
    std::string str;
    str += "SidechainDB:\n";
    str += "Active sidechains: ";
    str += (int)vActiveSidechain.size();
    str += "\n";
    for (const Sidechain& s : vActiveSidechain) {
        // Print sidechain name
        str += "Sidechain: " + s.GetSidechainName() + "\n";
        // Print sidechain WT^ workscore(s)
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        str += "WT^(s): ";
        str += (int)vState.size();
        str += "\n";
        for (const SidechainWTPrimeState& state : vState) {
            str += "WT^:\n";
            str += state.ToString();
        }
        str += "\n";
    }
    return str;
}

bool SidechainDB::Update(int nHeight, const uint256& hashBlock, const uint256& hashPrevBlock, const std::vector<CTxOut>& vout, std::string& strError, bool fDebug)
{
    if (hashBlock.IsNull()) {
        if (fDebug)
            LogPrintf("%s: Failed: block hash is null at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!hashBlockLastSeen.IsNull() && hashPrevBlock.IsNull())
    {
        if (fDebug)
            LogPrintf("%s: Failed: previous block hash null at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!vout.size())
    {
        if (fDebug)
            LogPrintf("%s: Failed: empty coinbase transaction at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }

    if (!hashBlockLastSeen.IsNull() && hashPrevBlock != hashBlockLastSeen) {

        if (fDebug)
            LogPrintf("%s: Failed: previous block hash: %s does not match hashBlockLastSeen: %s at height: %u\n",
                    __func__,
                    hashPrevBlock.ToString(),
                    hashBlockLastSeen.ToString(),
                    nHeight);
        return false;
    }

    // If the WT^ verification period ended, clear old data
    if (nHeight > 0 && (nHeight % SIDECHAIN_VERIFICATION_PERIOD) == 0) {
        if (fDebug)
            LogPrintf("%s: WT^ status reset (expired) at height: %u\n",
                    __func__,
                    nHeight);
        ResetWTPrimeState();
    }

    /*
     * Look for data relevant to SCDB in this block's coinbase.
     *
     * Scan for new WT^(s) and start tracking them.
     *
     * Scan for updated SCDB MT hash, and perform MT hash based SCDB update.
     *
     * Scan for sidechain proposals & sidechain activation commitments.
     *
     * Update hashBlockLastSeen.
     */

    // Scan for sidechain proposal commitments
    std::vector<SidechainProposal> vProposal;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        if (!scriptPubKey.IsSidechainProposalCommit())
            continue;

        SidechainProposal proposal;
        if (!proposal.DeserializeFromScript(scriptPubKey))
            continue; // TODO return false?

        // Check for duplicate
        bool fDuplicate = false;
        for (const SidechainActivationStatus& s : vActivationStatus) {
            if (s.proposal == proposal) {
                fDuplicate = true;
                break;
            }
        }
        if (fDuplicate)
            continue;

        vProposal.push_back(proposal);
    }
    if (vProposal.size() == 1) {
        SidechainActivationStatus status;
        status.nFail = 0;
        status.nAge = 0;
        status.proposal = vProposal.front();

        // Make sure that the proposal is unique,
        bool fUnique = true;

        // check the activation status cache
        for (const SidechainActivationStatus& s : vActivationStatus) {
            if (s.proposal == status.proposal) {
                fUnique = false;
                break;
            }
        }
        // check the active sidechain list
        for (const Sidechain& s : vActiveSidechain) {
            // Note that we are comparing a Sidechain to a SidechainProposal.
            // There is a custom operator== for this purpose.
            if (s == status.proposal) {
                fUnique = false;
                break;
            }
        }

        if (fUnique)
            vActivationStatus.push_back(status);
    }

    // Scan for sidechain activation commitments
    std::vector<uint256> vActivationHash;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        if (!scriptPubKey.IsSidechainActivationCommit())
            continue;

        uint256 hash = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 5, scriptPubKey.begin() + 37));
        if (hash.IsNull())
            continue;

        vActivationHash.push_back(hash);
    }
    UpdateActivationStatus(vActivationHash);

    // Scan for new WT^(s) and start tracking them
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsWTPrimeHashCommit()) {
            // Get WT^ hash from script
            uint256 hashWTPrime = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));

            // Get sidechain number
            uint8_t nSidechain = scriptPubKey[38];
            if (!IsSidechainNumberValid(nSidechain)) {
                if (fDebug)
                    LogPrintf("%s: Skipping new WT^: %s, invalid sidechain number: %u\n",
                            __func__,
                            hashWTPrime.ToString(),
                            nSidechain);
                continue;
            }

            if (!AddWTPrime(nSidechain, hashWTPrime, nHeight, fDebug)) {
                // TODO handle failure
                if (fDebug) {
                    LogPrintf("%s: Failed to cache WT^: %s for sidechain number: %u at height: %u\n",
                            __func__,
                            hashWTPrime.ToString(),
                            nSidechain,
                            nHeight);
                }
                continue;
            }
        }
    }

    // Scan for updated SCDB MT hash and try to update workscore of WT^(s)
    std::vector<CScript> vMTHashScript;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsSCDBHashMerkleRootCommit())
            vMTHashScript.push_back(scriptPubKey);
    }

    // Only one MT hash commit is allowed per coinbase
    if (vMTHashScript.size() == 1) {
        const CScript& scriptPubKey = vMTHashScript.front();

        // Get MT hash from script
        uint256 hashMerkleRoot = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));
        bool fUpdated = UpdateSCDBMatchMT(nHeight, hashMerkleRoot);
        // TODO handle !fUpdated

        if (!fUpdated && fDebug) {
            LogPrintf("%s: Failed to match MT: %s at height: %u\n",
                    __func__,
                    hashMerkleRoot.ToString(),
                    nHeight);
        }
    }

    // Update hashBLockLastSeen
    hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight, bool fDebug)
{
    if (vNewScores.empty()) {
        if (fDebug)
            LogPrintf("%s: Update failed! No new scores at height: %u\n",
                    __func__,
                    nHeight);
        return false;
    }
    if (vWTPrimeStatus.empty()) {
        if (fDebug)
            LogPrintf("%s: Update failed: vWTPrimeStatus is empty!\n",
                    __func__);
        return false;
    }

    // First check that sidechain numbers are valid
    for (const SidechainWTPrimeState& s : vNewScores) {
        if (!IsSidechainNumberValid(s.nSidechain)) {
            if (fDebug)
                LogPrintf("%s: Update failed! Invalid sidechain number: %u\n",
                        __func__,
                        s.nSidechain);
            return false;
        }
    }

    // Decrement nBlocksLeft of existing WT^(s)
    for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
        for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
            vWTPrimeStatus[x][y].nBlocksLeft--;
        }
    }

    // TODO
    // keep a list of the work scores that get updated, their
    // blocks remaining should have been updated as well.
    // After that, loop through again and update the
    // blocks remaining of any WT^ that wasn't in the list
    // that had their workscores updated.

    // Apply new work scores
    for (const SidechainWTPrimeState& s : vNewScores) {
        bool fFound = false;
        for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
            // TODO change containers, improve efficiency
            // If we found the WT^, this is an update. Otherwise we are adding
            // a new one.
            for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
                const SidechainWTPrimeState state = vWTPrimeStatus[x][y];
                if (state.hashWTPrime == s.hashWTPrime) {
                    fFound = true;
                    if ((state.nWorkScore == s.nWorkScore) ||
                            (s.nWorkScore == (state.nWorkScore + 1)) ||
                            (s.nWorkScore == (state.nWorkScore - 1)))
                    {
                        if (fDebug)
                            LogPrintf("%s: WT^ work  score updated: %s %u->%u\n",
                                    __func__,
                                    state.hashWTPrime.ToString(),
                                    vWTPrimeStatus[x][y].nWorkScore,
                                    s.nWorkScore);
                        vWTPrimeStatus[x][y].nWorkScore = s.nWorkScore;
                    }
                }
            }
        }
        if (!fFound) {
            if (s.nWorkScore != 1) {
                if (fDebug)
                    LogPrintf("%s: Rejected new WT^: %s. Invalid initial workscore (not 1): %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            s.nWorkScore);
                continue;
            }

            int nAge = GetNumBlocksSinceLastSidechainVerificationPeriod(nHeight);
            if (s.nBlocksLeft != (SIDECHAIN_VERIFICATION_PERIOD - nAge)) {
                if (fDebug)
                    LogPrintf("%s: Rejected new WT^: %s. Invalid initial nBlocksLeft (not %u): %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            SIDECHAIN_VERIFICATION_PERIOD - nAge,
                            s.nBlocksLeft);
                continue;
            }

            if (!IsSidechainNumberValid(s.nSidechain)) {
                if (fDebug)
                    LogPrintf("%s: Rejected new WT^: %s. Invalid sidechain number: %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            s.nSidechain);
                continue;
            }

            if (s.nSidechain >= vWTPrimeStatus.size()) {
                if (fDebug)
                    LogPrintf("%s: Rejected new WT^: %s. Invalid sidechain number (too large): %u\n",
                            __func__,
                            s.hashWTPrime.ToString(),
                            s.nSidechain);
                continue;
            }

            vWTPrimeStatus[s.nSidechain].push_back(s);

            if (fDebug)
                LogPrintf("%s: Cached new WT^: %s\n",
                        __func__,
                        s.hashWTPrime.ToString());
        }
    }

    if (fDebug)
        LogPrintf("%s: Finished updating at height: %u with %u WT^ updates.\n",
                __func__,
                nHeight,
                vNewScores.size());

    return true;
}

bool SidechainDB::UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot)
{
    // First see if we are already synchronized
    if (GetSCDBHash() == hashMerkleRoot)
        return true;

    // Try testing out most likely updates
    std::vector<SidechainWTPrimeState> vUpvote = GetVotes(SCDB_UPVOTE);
    if (GetSCDBHashIfUpdate(vUpvote, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vUpvote, nHeight, true /* fDebug */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vAbstain = GetVotes(SCDB_ABSTAIN);
    if (GetSCDBHashIfUpdate(vAbstain, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vAbstain, nHeight, true /* fDebug */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vDownvote = GetVotes(SCDB_DOWNVOTE);
    if (GetSCDBHashIfUpdate(vDownvote, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vDownvote, nHeight, true /* fDebug */);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    // TODO remove
    // Try to update based on network messages
    for (const SidechainUpdatePackage& update : vSidechainUpdateCache) {
        if (update.nHeight != nHeight)
            continue;

        // Create WTPrimeState objects from the update message
        std::vector<SidechainWTPrimeState> vWT;
        for (const SidechainUpdateMSG& msg : update.vUpdate) {
            // Is sidechain number valid?
            if (!IsSidechainNumberValid(msg.nSidechain))
                 return false;

            SidechainWTPrimeState wt;
            wt.nSidechain = msg.nSidechain;
            wt.hashWTPrime = msg.hashWTPrime;
            wt.nWorkScore = msg.nWorkScore;
            wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD;

            // Lookup the old state (for nBlocksLeft) TODO do this better
            std::vector<SidechainWTPrimeState> vOld = GetState(wt.nSidechain);
            for (const SidechainWTPrimeState& old : vOld) {
                if (wt == old)
                    wt.nBlocksLeft = old.nBlocksLeft - 1;
            }

            vWT.push_back(wt);
        }

        // Test out updating SCDB copy with this update package
        // if it worked, apply the update
        if (GetSCDBHashIfUpdate(vWT, nHeight) == hashMerkleRoot) {
            UpdateSCDBIndex(vWT, nHeight, true /* fDebug */);
            return (GetSCDBHash() == hashMerkleRoot);
        }
    }
    return false;
}

bool SidechainDB::ApplyDefaultUpdate()
{
    if (!HasState())
        return true;

    // Decrement nBlocksLeft, nothing else changes
    for (size_t x = 0; x < vWTPrimeStatus.size(); x++) {
        for (size_t y = 0; y < vWTPrimeStatus[x].size(); y++) {
            vWTPrimeStatus[x][y].nBlocksLeft--;
        }
    }

    return true;
}

void SidechainDB::UpdateActivationStatus(const std::vector<uint256>& vHash)
{
    // Increment the age of all sidechain proposals, remove expired.
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        vActivationStatus[i].nAge++;
        if (vActivationStatus[i].nAge > SIDECHAIN_ACTIVATION_MAX_AGE) {
            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();
        }
    }

    // Calculate failures. Sidechain proposals with activation status will have
    // their activation failure count increased by 1 if a activation commitment
    // for them is not found in the block.
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        bool fFound = false;
        for (const uint256& u : vHash) {
            if (u == vActivationStatus[i].proposal.GetHash()) {
                fFound = true;
                break;
            }
        }
        if (!fFound)
            vActivationStatus[i].nFail++;
    }

    // Remove sidechain proposals with too many failures to activate
    std::vector<std::vector<SidechainActivationStatus>::const_iterator> vFail;
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nFail >= SIDECHAIN_ACTIVATION_MAX_FAILURES) {
            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();
        }
    }

    // TODO this needs to be replaced
    // Don't activate any more sidechains if we have reached the max
    if (vActiveSidechain.size() >= SIDECHAIN_ACTIVATION_MAX_ACTIVE)
        return;

    // Move activated sidechains to vActivatedSidechain
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nAge == SIDECHAIN_ACTIVATION_MAX_AGE) {
            // Create sidechain object
            Sidechain sidechain;
            sidechain.nVersion = vActivationStatus[i].proposal.nVersion;
            sidechain.hashID1 = vActivationStatus[i].proposal.hashID1;
            sidechain.hashID2 = vActivationStatus[i].proposal.hashID2;
            // TODO Get nSidechain in a smarter way
            sidechain.nSidechain = vActiveSidechain.size();
            sidechain.sidechainPriv = vActivationStatus[i].proposal.sidechainPriv;
            sidechain.sidechainHex = vActivationStatus[i].proposal.sidechainHex;
            sidechain.sidechainKeyID = vActivationStatus[i].proposal.sidechainKeyID;
            sidechain.title = vActivationStatus[i].proposal.title;
            sidechain.description = vActivationStatus[i].proposal.description;

            vActiveSidechain.push_back(sidechain);

            // Save proposal for later
            SidechainProposal proposal = vActivationStatus[i].proposal;

            vActivationStatus[i] = vActivationStatus.back();
            vActivationStatus.pop_back();

            // Add blank vector to track this sidechain's WT^(s)
            vWTPrimeStatus.push_back(std::vector<SidechainWTPrimeState>{});

            // Remove proposal from our cache if it has activated
            for (size_t j = 0; j < vSidechainProposal.size(); j++) {
                if (proposal == vSidechainProposal[j]) {
                    vSidechainProposal[j] = vSidechainProposal.back();
                    vSidechainProposal.pop_back();
                }
            }
        }
    }
}

void SidechainDB::UndoActivationStatusUpdate(const std::vector<uint256>& vHash)
{
    // TODO
    // TODO Refactor: improve by changing containers?
    for (const uint256& u : vHash) {
    }
}


int GetLastSidechainVerificationPeriod(int nHeight)
{
    for (;;) {
        if (nHeight < 0)
            return -1;
        if (nHeight == 0 || nHeight % SIDECHAIN_VERIFICATION_PERIOD == 0)
            break;
        nHeight--;
    }
    return nHeight;
}

int GetNumBlocksSinceLastSidechainVerificationPeriod(int nHeight)
{
    int nPeriodStart = GetLastSidechainVerificationPeriod(nHeight);
    return nHeight - nPeriodStart;
}
