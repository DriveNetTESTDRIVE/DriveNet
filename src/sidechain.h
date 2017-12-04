// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAIN_H
#define BITCOIN_SIDECHAIN_H

#include "primitives/transaction.h"
#include "pubkey.h"

#include <array>

//! Max number of WT^(s) per sidechain during tau
static const int SIDECHAIN_MAX_WT = 3; // TODO remove
static const size_t VALID_SIDECHAINS_COUNT = 3;

enum SidechainNumber {
    SIDECHAIN_TEST = 0,
    SIDECHAIN_HIVEMIND = 1,
    SIDECHAIN_WIMBLE = 2
};

struct Sidechain {
    uint8_t nSidechain;
    uint16_t nWaitPeriod;
    uint16_t nVerificationPeriod;
    uint16_t nMinWorkScore;
    const char* sidechainKey;
    const char* sidechainPriv;
    const char* sidechainHex;

    std::string GetSidechainName() const;
    uint16_t GetTau() const;
    // Return height of the end of previous / beginning of current tau
    int GetLastTauHeight(int nHeight) const;
    bool operator==(const Sidechain& a) const;
    std::string ToString() const;
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CMutableTransaction tx;

    bool operator==(const SidechainDeposit& a) const;
    std::string ToString() const;
};

struct SidechainLD {
    int8_t nSidechain;
    int8_t nPrevBlockRef;
    uint256 hashCritical;

    bool operator==(const SidechainLD& a) const;
    uint256 GetHash(void) const;

    // For hash calculation
    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(nPrevBlockRef);
        READWRITE(hashCritical);
    }
};

struct SidechainUpdateMSG {
    uint8_t nSidechain;
    uint256 hashWTPrime;
    uint16_t nWorkScore;
};

struct SidechainUpdatePackage {
    int nHeight;
    std::vector<SidechainUpdateMSG> vUpdate;
};

struct SidechainWTPrimeState {
    uint8_t nSidechain;
    uint16_t nBlocksLeft;
    uint16_t nWorkScore;
    uint256 hashWTPrime;

    bool IsNull() const;
    uint256 GetHash(void) const;
    bool operator==(const SidechainWTPrimeState& a) const;
    std::string ToString() const;

    // For hash calculation
    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(nBlocksLeft);
        READWRITE(nWorkScore);
        READWRITE(hashWTPrime);
    }
};

struct SCDBIndex {
    std::array<SidechainWTPrimeState, SIDECHAIN_MAX_WT> members;
    bool IsPopulated() const;
    bool IsFull() const;
    bool InsertMember(const SidechainWTPrimeState& member);
    void ClearMembers();
    unsigned int CountPopulatedMembers() const;
    bool Contains(uint256 hashWT) const;
    bool GetMember(uint256 hashWT, SidechainWTPrimeState& wt) const;
};

static const std::array<Sidechain, VALID_SIDECHAINS_COUNT> ValidSidechains =
{{
    // {nSidechain, nWaitPeriod, nVerificationPeriod, nMinWorkScore, sidechainKey, sidechainPriv, sidechainHex}
    {SIDECHAIN_TEST, 100, 200, 100, "09c1fbf0ad3047fb825e0bc5911528596b7d7f49", "cQMQ99mA5Xi2Hm9YM3WmB2JcJai3tzGupuFb5b7HWiwNgTKoaFr5", "76a914497f7d6b59281591c50b5e82fb4730adf0fbc10988ac"},
    {SIDECHAIN_HIVEMIND, 100, 200, 100, "09c1fbf0ad3047fb825e0bc5911528596b7d7f49", "cQMQ99mA5Xi2Hm9YM3WmB2JcJai3tzGupuFb5b7HWiwNgTKoaFr5", "76a914497f7d6b59281591c50b5e82fb4730adf0fbc10988ac"},
    {SIDECHAIN_WIMBLE, 100, 200, 100, "09c1fbf0ad3047fb825e0bc5911528596b7d7f49", "cQMQ99mA5Xi2Hm9YM3WmB2JcJai3tzGupuFb5b7HWiwNgTKoaFr5", "76a914497f7d6b59281591c50b5e82fb4730adf0fbc10988ac"}
}};


bool IsSidechainNumberValid(uint8_t nSidechain);

std::string GetSidechainName(uint8_t nSidechain);

#endif // BITCOIN_SIDECHAIN_H
