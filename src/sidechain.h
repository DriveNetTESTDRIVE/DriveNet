// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAIN_H
#define BITCOIN_SIDECHAIN_H

#include <primitives/transaction.h>
#include <pubkey.h>

#include <array>

//! Max number of WT^(s) per sidechain during verification period
static const int SIDECHAIN_MAX_WT = 3; // TODO remove
static const size_t VALID_SIDECHAINS_COUNT = 5;
static const int SIDECHAIN_VERIFICATION_PERIOD = 26298;
static const int SIDECHAIN_MIN_WORKSCORE = 13140;
static const int SIDECHAIN_TEST_MIN_WORKSCORE = 6; // TODO remove
static const int SIDECHAIN_TEST_VERIFICATION_PERIOD = 35; // TODO remove

enum SidechainNumber {
    SIDECHAIN_TEST = 0,
    SIDECHAIN_HIVEMIND = 1,
    SIDECHAIN_WIMBLE = 2,
    SIDECHAIN_CASH = 3,
    SIDECHAIN_ROOTSTOCK = 4
};

struct Sidechain {
    uint8_t nSidechain;
    const char* sidechainKey;
    const char* sidechainPriv;
    const char* sidechainHex;

    std::string GetSidechainName() const;
    // Return height of the beginning of current verification period
    int GetLastVerificationPeriod(int nHeight) const;
    bool operator==(const Sidechain& a) const;
    std::string ToString() const;
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CMutableTransaction tx;
    uint32_t n;

    bool operator==(const SidechainDeposit& a) const;
    std::string ToString() const;
};

struct SidechainLD {
    uint8_t nSidechain;
    uint16_t nPrevBlockRef;
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
    // {nSidechain, sidechainKey, sidechainPriv, sidechainHex}
    {SIDECHAIN_TEST,        "4f63ac20e97ea2d44faa0212d0a26dff53ed5dca", "cPNEJzi2Q9m4R1jhNyL1uq6ABRqooFsSvTbMeAWb4d9EArVNLhjs", "76a914ca5ded53ff6da2d01202aa4fd4a27ee920ac634f88ac"},
    {SIDECHAIN_HIVEMIND,    "47a38ea92c81bb39d6aa128b81ba1c9621cda471", "cTEHu8V8S5eHWutKawHr62YKfGuC6sq2HS877UHntUHKUKdQ7NLt", "76a9145246d81d43dca6f29cacbdb21c70e438a41b0d1288ac"},
    {SIDECHAIN_WIMBLE,      "daed6490f7802cb1f0a9653940926b67fbb86a1f", "cW1ZpMUi1Hz2R4Edj2c9WVCuypf3ycLEbs4gxEWo9y79qWWDxrbG", "76a9141f6ab8fb676b92403965a9f0b12c80f79064edda88ac"},
    {SIDECHAIN_CASH,        "6c5cb8dff6217b74f5b1c73c7d2722931e3674a8", "cQiAEdTGCiGZf64cPtRG6yBLB2pWaMGYyyU13uoeahKpZGLGEfvb", "76a914a874361e9322277d3cc7b1f5747b21f6dfb85c6c88ac"},
    {SIDECHAIN_ROOTSTOCK,   "5d9e4cf9b5dc9afe0cfd396e56e37d8991310d37", "cV6iGPhbYVrSeJkJdYwp8eFpKyVxYdx7JtVic4XshUqGsxUqyoon", "76a914370d3191897de3566e39fd0cfe9adcb5f94c9e5d88ac"}
}};

static const std::map<std::string, int> ValidSidechainField =
{
    {"76a914ca5ded53ff6da2d01202aa4fd4a27ee920ac634f88ac", SIDECHAIN_TEST},
    {"76a9145246d81d43dca6f29cacbdb21c70e438a41b0d1288ac", SIDECHAIN_HIVEMIND},
    {"76a9141f6ab8fb676b92403965a9f0b12c80f79064edda88ac", SIDECHAIN_WIMBLE},
    {"76a914a874361e9322277d3cc7b1f5747b21f6dfb85c6c88ac", SIDECHAIN_CASH},
    {"76a914370d3191897de3566e39fd0cfe9adcb5f94c9e5d88ac", SIDECHAIN_ROOTSTOCK}
};


bool IsSidechainNumberValid(uint8_t nSidechain);

std::string GetSidechainName(uint8_t nSidechain);

#endif // BITCOIN_SIDECHAIN_H
