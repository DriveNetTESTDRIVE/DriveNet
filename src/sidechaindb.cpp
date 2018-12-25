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
        for (size_t i = 0; i < tx.vout.size(); i++) {
            const CScript &scriptPubKey = tx.vout[i].scriptPubKey;
            uint8_t nSidechainTmp;
            if (HasSidechainScript(std::vector<CScript>{scriptPubKey}, nSidechainTmp)) {
                // Copy output index of deposit burn and move on
                deposit.n = i;
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
        if (CTransaction(deposit.tx) == tx) {
            vDeposit.push_back(deposit);
        }
    }

    // Add deposits to cache
    for (const SidechainDeposit& d : vDeposit) {
        if (!HaveDepositCached(d))
            vDepositCache.push_back(d);
    }
}

void SidechainDB::AddDeposits(const std::vector<SidechainDeposit>& vDeposit)
{
    for (const SidechainDeposit& d : vDeposit) {
        vDepositCache.push_back(d);
    }
}

void SidechainDB::AddSidechainNetworkUpdatePackage(const SidechainUpdatePackage& update)
{
    vSidechainUpdateCache.push_back(update);
}

bool SidechainDB::AddWTPrime(uint8_t nSidechain, const uint256& hashWTPrime, int nHeight)
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    if (HaveWTPrimeWorkScore(hashWTPrime, nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.nSidechain = nSidechain;

    int nAge = GetNumBlocksSinceLastSidechainVerificationPeriod(nHeight);
    wt.nBlocksLeft = SIDECHAIN_VERIFICATION_PERIOD - nAge;
    wt.nWorkScore = 1;
    wt.hashWTPrime = hashWTPrime;

    vWT.push_back(wt);

    return UpdateSCDBIndex(vWT, nHeight);
}

void SidechainDB::CacheActiveSidechains(const std::vector<Sidechain>& vActiveSidechainIn)
{
    vActiveSidechain = vActiveSidechainIn;
}

void SidechainDB::CacheSidechainActivationStatus(const std::vector<SidechainActivationStatus>& vActivationStatusIn)
{
    vActivationStatus = vActivationStatusIn;
}

void SidechainDB::CacheSidechainProposals(const std::vector<SidechainProposal>& vSidechainProposalIn)
{
    vSidechainProposal = vSidechainProposalIn;
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

int SidechainDB::CountBlocksAtop(const CCriticalData& data) const
{
    uint8_t nSidechain;
    uint16_t nPrevBlockRef;
    if (!data.IsBMMRequest(nSidechain, nPrevBlockRef))
        return 0;

    // Translate critical data into LD
    SidechainLD ld;
    ld.nSidechain = nSidechain;
    ld.nPrevBlockRef = nPrevBlockRef;
    ld.hashCritical = data.hashCritical;

    return CountBlocksAtop(ld);
}

int SidechainDB::CountBlocksAtop(const SidechainLD& ld) const
{
    if (!IsSidechainNumberValid(ld.nSidechain))
        return 0;

    // Count blocks atop (side:block confirmations in ratchet)
    for (size_t i = 0; i < ratchet[ld.nSidechain].size(); i++) {
        if (ratchet[ld.nSidechain][i] == ld) {
            return ratchet[ld.nSidechain].size() - i;
        }
    }
    return 0;
}

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime) {
            if (state.nWorkScore >= SIDECHAIN_MIN_WORKSCORE) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

int SidechainDB::GetActiveSidechainCount() const
{
    return vActiveSidechain.size();
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(uint8_t nSidechain) const
{
    // TODO C++11 const for loop
    std::vector<SidechainDeposit> vSidechainDeposit;
    for (size_t i = 0; i < vDepositCache.size(); i++) {
        if (vDepositCache[i].nSidechain == nSidechain)
            vSidechainDeposit.push_back(vDepositCache[i]);
    }
    return vSidechainDeposit;
}

uint256 SidechainDB::GetBMMHash() const
{
    std::vector<uint256> vLeaf;
    for (const auto& a : ratchet) {
        for (const SidechainLD& ld : a) {
            vLeaf.push_back(ld.GetHash());
        }
    }
    return ComputeMerkleRoot(vLeaf);
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

uint256 SidechainDB::GetHashBlockLastSeen()
{
    return hashBlockLastSeen;
}

uint256 SidechainDB::GetSCDBHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight) const
{
    SidechainDB scdbCopy = (*this);
    scdbCopy.UpdateSCDBIndex(vNewScores, nHeight);

    return (scdbCopy.GetSCDBHash());
}

bool SidechainDB::GetLinkingData(uint8_t nSidechain, std::vector<SidechainLD>& ld) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    if (nSidechain >= ratchet.size())
        return false;

    ld = ratchet[nSidechain];

    return true;
}

bool SidechainDB::GetSidechainScript(const uint8_t nSidechain, CScript& scriptPubKey) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    for (const Sidechain& s : vActiveSidechain) {
        if (s.nSidechain == nSidechain) {
            std::vector<unsigned char> vch(ParseHex(s.sidechainHex));
            scriptPubKey = CScript(vch.begin(), vch.end());
            return true;
        }
    }
    return false;
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

std::vector<SidechainWTPrimeState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !IsSidechainNumberValid(nSidechain))
        return std::vector<SidechainWTPrimeState>();

    std::vector<SidechainWTPrimeState> vState;
    for (const SCDBIndex& i : SCDB) {
        for (const SidechainWTPrimeState& member : i.members) {
            if (!member.IsNull())
                vState.push_back(member);
        }
    }
    return vState;
}

std::vector<CTransaction> SidechainDB::GetWTPrimeCache() const
{
    return vWTPrimeCache;
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

bool SidechainDB::HasState() const
{
    // Make sure that SCDB is actually initialized
    if (SCDB.empty() || !GetActiveSidechainCount())
        return false;

    // Check if any SCDBIndex(s) are populated
    for (const SCDBIndex& i : SCDB) {
        if (i.IsPopulated())
            return true;
    }

    if (vWTPrimeCache.size())
        return true;

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

bool SidechainDB::HaveLinkingData(uint8_t nSidechain, uint256 hashCritical) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    for (const SidechainLD& ld : ratchet[nSidechain]) {
        if (ld.hashCritical == hashCritical)
            return true;
    }
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

void SidechainDB::ResetWTPrimeState()
{
    // Clear out SCDB
    SCDB.clear();
    SCDB.resize(vActiveSidechain.size());

    // Clear out BMM LD
    ratchet.clear();

    // Clear out cached WT^(s)
    vWTPrimeCache.clear();

    // Reset hashBlockLastSeen
    hashBlockLastSeen.SetNull();
}

void SidechainDB::ResetSidechains()
{
    // Clear out active sidechains
    vActiveSidechain.clear();

    // Clear out sidechain activation status
    vActivationStatus.clear();
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

bool SidechainDB::Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError)
{
    if (hashBlock.IsNull())
        return false;
    if (!vout.size())
        return false;

    // If the verification period ended, clear old data
    if (nHeight > 0 && (nHeight % SIDECHAIN_VERIFICATION_PERIOD) == 0) {
        SCDB.clear();
        SCDB.resize(GetActiveSidechainCount());
        vWTPrimeCache.clear();
    }

    /*
     * Now we will look for data that is relevant to SCDB
     * in this block's coinbase.
     *
     * Scan for h* linking data and add it to the BMMLD
     * ratchet system.
     *
     * Scan for new WT^(s) and start tracking them.
     *
     * Scan for updated SCDB MT hash, and perform MT hash
     * based SCDB update.
     *
     * Update hashBlockLastSeen to reflect that we have
     * scanned this latest block.
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

        // TODO refactor: change containers to improve this?
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

    // Scan for bmm h*(s)
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        if (!scriptPubKey.IsCriticalHashCommit())
            continue;

        // Read critical data bytes if there are any
        if (scriptPubKey.size() > 38) {
            CCriticalData criticalData;
            criticalData.hashCritical = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));

            // Do the bytes indicate that this is a bmm h*?
            uint8_t nSidechain;
            uint16_t nPrevBlockRef;
            if (!criticalData.IsBMMRequest(nSidechain, nPrevBlockRef))
                continue;

            if (nPrevBlockRef > ratchet[nSidechain].size())
                continue;

            SidechainLD ld;
            ld.nSidechain = nSidechain;
            ld.hashCritical = criticalData.hashCritical;
            ld.nPrevBlockRef = nPrevBlockRef;

            ratchet[nSidechain].push_back(ld);

            // Maintain ratchet size limit
            if (!(ratchet[nSidechain].size() < BMM_MAX_LD)) {
                // TODO change to vector of queue for pop() ?
                ratchet.erase(ratchet.begin());
            }
        }
    }

    // Scan for new WT^(s) and start tracking them
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsWTPrimeHashCommit()) {
            // Get WT^ hash from script
            uint256 hashWTPrime = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));

            // Get sidechain number
            std::vector<unsigned char> vchNS;
            vchNS.push_back(scriptPubKey[39]);

            CScriptNum nSidechain(vchNS, false);

            if (!AddWTPrime(nSidechain.getint(), hashWTPrime, nHeight)) {
                // TODO handle failure or at least log something
                continue;
            }
        }
    }

    // Scan for updated SCDB MT hash and try to update
    // workscore of WT^(s)
    // Note: h*(s) and new WT^(s) must be added to SCDB
    // before this can be done.
    // Note: Only one MT hash commit is allowed per coinbase
    std::vector<CScript> vMTHashScript;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsSCDBHashMerkleRootCommit())
            vMTHashScript.push_back(scriptPubKey);
    }

    if (vMTHashScript.size() == 1) {
        const CScript& scriptPubKey = vMTHashScript.front();

        // Get MT hash from script
        uint256 hashMerkleRoot = uint256(std::vector<unsigned char>(scriptPubKey.begin() + 6, scriptPubKey.begin() + 38));
        bool fUpdated = UpdateSCDBMatchMT(nHeight, hashMerkleRoot);
        // TODO handle !fUpdated
    }

    // Update hashBLockLastSeen
    hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores, int nHeight)
{
    if (!vNewScores.size())
        return false;
    if (SCDB.empty()) {
        return false;
    }

    // First check that sidechain numbers are valid
    for (const SidechainWTPrimeState& s : vNewScores) {
        if (!IsSidechainNumberValid(s.nSidechain)) {
            return false;
        }
    }

    // Decrement nBlocksLeft of existing WT^(s)
    for (auto i : SCDB) {
        for (SidechainWTPrimeState wt : i.members) {
            if (!wt.IsNull()) {
                // wt is a copy
                wt.nBlocksLeft--;
                i.InsertMember(wt);
            }
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
        for (auto i : SCDB) {
            if (i.nSidechain == s.nSidechain) {
                SidechainWTPrimeState wt;
                if (i.GetMember(s.hashWTPrime, wt)) {
                    // Update an existing WT^
                    // Check that new work score is valid
                    if ((wt.nWorkScore == s.nWorkScore) ||
                        (s.nWorkScore == (wt.nWorkScore + 1)) ||
                        (s.nWorkScore == (wt.nWorkScore - 1)))
                    {
                        bool fInserted = i.InsertMember(s);
                        // TODO handle fInserted == false
                    }
                }
                else
                if (!i.IsFull()) {
                    // Add a new WT^
                    if (s.nWorkScore != 1)
                        continue;

                    int nAge = GetNumBlocksSinceLastSidechainVerificationPeriod(nHeight);
                    if (s.nBlocksLeft != (SIDECHAIN_VERIFICATION_PERIOD - nAge))
                        continue;

                    bool fInserted = i.InsertMember(s);
                }
            }
        }
    }
    return true;
}

bool SidechainDB::UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot)
{
    // First see if we are already synchronized
    if (GetSCDBHash() == hashMerkleRoot)
        return true;

    // Try testing out most likely updates
    std::vector<SidechainWTPrimeState> vUpvote = GetUpvotes();
    if (GetSCDBHashIfUpdate(vUpvote, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vUpvote, nHeight);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vAbstain = GetAbstainVotes();
    if (GetSCDBHashIfUpdate(vAbstain, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vAbstain, nHeight);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    std::vector<SidechainWTPrimeState> vDownvote = GetDownvotes();
    if (GetSCDBHashIfUpdate(vDownvote, nHeight) == hashMerkleRoot) {
        UpdateSCDBIndex(vDownvote, nHeight);
        return (GetSCDBHash() == hashMerkleRoot);
    }

    // TODO the loop below is functional. It isn't efficient.
    // Changing the container of the update cache might be a good
    // place to start.
    //
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
            UpdateSCDBIndex(vWT, nHeight);
            return (GetSCDBHash() == hashMerkleRoot);
        }
    }
    return false;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetDownvotes() const
{
    std::vector<SidechainWTPrimeState> vNew;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vOld = GetState(s.nSidechain);

        if (!vOld.size())
            continue;

        SidechainWTPrimeState latest = vOld.back();
        latest.nBlocksLeft--;
        latest.nWorkScore--;

        vNew.push_back(latest);
    }
    return vNew;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetAbstainVotes() const
{
    std::vector<SidechainWTPrimeState> vNew;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vOld = GetState(s.nSidechain);

        if (!vOld.size())
            continue;

        SidechainWTPrimeState latest = vOld.back();
        latest.nBlocksLeft--;

        vNew.push_back(latest);
    }
    return vNew;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetUpvotes() const
{
    std::vector<SidechainWTPrimeState> vNew;
    for (const Sidechain& s : vActiveSidechain) {
        std::vector<SidechainWTPrimeState> vOld = GetState(s.nSidechain);

        if (!vOld.size())
            continue;

        SidechainWTPrimeState latest = vOld.back();
        latest.nBlocksLeft--;
        latest.nWorkScore++;

        vNew.push_back(latest);
    }
    return vNew;
}

std::vector<SidechainActivationStatus> SidechainDB::GetSidechainActivationStatus() const
{
    return vActivationStatus;
}

std::vector<Sidechain> SidechainDB::GetActiveSidechains() const
{
    return vActiveSidechain;
}

std::vector<SidechainProposal> SidechainDB::GetSidechainProposals() const
{
    return vSidechainProposal;
}

std::string SidechainDB::GetSidechainName(uint8_t nSidechain) const
{
    std::string str = "UnknownSidechain";

    Sidechain sidechain;
    if (GetSidechain(nSidechain, sidechain))
        return sidechain.title;

    return str;
}

bool SidechainDB::ApplyDefaultUpdate()
{
    if (!HasState())
        return true;

    // Decrement nBlocksLeft, nothing else changes
    for (auto i : SCDB) {
        for (SidechainWTPrimeState wt : i.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            i.InsertMember(wt);
        }
    }
    return true;
}

void SidechainDB::UpdateActivationStatus(const std::vector<uint256>& vHash)
{
    // TODO refactor, focusing on clarity over performance right now.

    // Increment the age of all sidechains, keep track of which have expired.
    std::vector<std::vector<SidechainActivationStatus>::const_iterator> vExpired;
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        vActivationStatus[i].nAge++;
        if (vActivationStatus[i].nAge > SIDECHAIN_ACTIVATION_MAX_AGE) {
            std::vector<SidechainActivationStatus>::const_iterator it = vActivationStatus.begin() + i;
            vExpired.push_back(it);
        }
    }
    // Remove expired sidechain proposals
    for (size_t i = 0; i < vExpired.size(); i++)
        vActivationStatus.erase(vExpired[i]);

    // Calculate failures. Sidechain proposals in the proposal cache which do
    // not have a sidechain activation commitment in this block will have their
    // activation failure count incremented by 1.
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

    // Make a list of proposals with too many failures to activate
    std::vector<std::vector<SidechainActivationStatus>::const_iterator> vFail;
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nFail >= SIDECHAIN_ACTIVATION_MAX_FAILURES) {
            std::vector<SidechainActivationStatus>::const_iterator it = vActivationStatus.begin() + i;
            vFail.push_back(it);
        }
    }
    // Remove sidechain proposals with too many failures to activate
    for (size_t i = 0; i < vFail.size(); i++) {
        vActivationStatus.erase(vFail[i]);
    }

    // TODO this needs to be replaced
    // Don't activate any more sidechains if we have reached the max
    if (vActiveSidechain.size() >= SIDECHAIN_ACTIVATION_MAX_ACTIVE)
        return;

    // Move activated sidechains to vActivatedSidechain
    std::vector<std::vector<SidechainActivationStatus>::const_iterator> vActivated;
    std::vector<std::vector<SidechainProposal>::const_iterator> vActivatedMine;
    for (size_t i = 0; i < vActivationStatus.size(); i++) {
        if (vActivationStatus[i].nAge == SIDECHAIN_ACTIVATION_MAX_AGE) {
            // Create sidechain object
            Sidechain sidechain;
            // TODO this also needs to be replaced
            sidechain.nSidechain = vActiveSidechain.size();
            sidechain.sidechainPriv = vActivationStatus[i].proposal.sidechainPriv;
            sidechain.sidechainHex = vActivationStatus[i].proposal.sidechainHex;
            sidechain.sidechainKey = vActivationStatus[i].proposal.sidechainKey;
            sidechain.title = vActivationStatus[i].proposal.title;
            sidechain.description = vActivationStatus[i].proposal.description;

            vActiveSidechain.push_back(sidechain);

            std::vector<SidechainActivationStatus>::const_iterator it = vActivationStatus.begin() + i;
            vActivated.push_back(it);

            // Add SCDBIndex to SCDB to track this sidechain's WT^(s)
            SCDBIndex index;
            index.nSidechain = sidechain.nSidechain;
            SCDB.push_back(index);

            // Did one of our cached proposals activate?
            for (size_t j = 0; j < vSidechainProposal.size(); j++) {
                if (vActivationStatus[i].proposal == vSidechainProposal[j]) {
                    std::vector<SidechainProposal>::const_iterator it2 = vSidechainProposal.begin() + j;
                    vActivatedMine.push_back(it2);
                }
            }
        }
    }
    // TODO change container / set value instead of erase. Vector erase is slow.
    // Remove sidechain proposals which have activated.
    for (size_t i = 0; i < vActivated.size(); i++) {
        vActivationStatus.erase(vActivated[i]);
    }
    // Remove our sidechain proposals that have activated from the cache
    for (size_t i = 0; i < vActivatedMine.size(); i++) {
        vSidechainProposal.erase(vActivatedMine[i]);
    }
}

void SidechainDB::UndoActivationStatusUpdate(const std::vector<uint256>& vHash)
{
    // TODO
    // TODO Refactor: improve by changing containers?
    for (const uint256& u : vHash) {
    }
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

int GetLastSidechainVerificationPeriod(int nHeight)
{
    // TODO more efficient
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
