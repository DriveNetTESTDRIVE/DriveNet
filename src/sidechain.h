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
    {SIDECHAIN_TEST,        "690571d5b3b11768bedce16b332e3099f1737534", "L22kLvHXmmtwLkhSVVyAus8HR6sD3meTuB1zNkFUJ43mMTAW642m", "76a914347573f199302e336be1dcbe6817b1b3d571056988ac"},
    {SIDECHAIN_HIVEMIND,    "7fdcbdadc76f01b474b7cf9ce1d253f0fb9adbcb", "L2dkgnU2tWtSm4teY95xjFrDENWLLZqG8NDfr3u6nkPE8KFAm5yZ", "76a914cbdb9afbf053d2e19ccfb774b4016fc7adbddc7f88ac"},
    {SIDECHAIN_WIMBLE,      "92cdebd5167167297d2b5293447e60c195162fc0", "L54Xj1NokdPBsaV8L5KEkx5nUUFvix8TcxNk6ZX5a5BoYhzMUGML", "76a914c02f1695c1607e4493522b7d29677116d5ebcd9288ac"},
    {SIDECHAIN_CASH,        "3b98766912b27100f13574a3ee6e1c7bcb53e782", "KwqwwiX8pbEN5FCDr9ho4NW1anX19hEv3omezS9sPgasLftsgBzR", "76a91482e753cb7b1c6eeea37435f10071b2126976983b88ac"},
    {SIDECHAIN_ROOTSTOCK,   "47a38ea92c81bb39d6aa128b81ba1c9621cda471", "L5Ads4cJ61b9YvHJWoa4mHYijWos9g4XyZoADrGuJrLAcaqXXeM6", "76a91471a4cd21961cba818b12aad639bb812ca98ea34788ac"}
}};

static const std::map<std::string, int> ValidSidechainField =
{
    {"76a914497f7d6b59281591c50b5e82fb4730adf0fbc10988ac", SIDECHAIN_TEST},
    {"76a914cbdb9afbf053d2e19ccfb774b4016fc7adbddc7f88ac", SIDECHAIN_HIVEMIND},
    {"76a914c02f1695c1607e4493522b7d29677116d5ebcd9288ac", SIDECHAIN_WIMBLE},
    {"76a91482e753cb7b1c6eeea37435f10071b2126976983b88ac", SIDECHAIN_CASH},
    {"76a91471a4cd21961cba818b12aad639bb812ca98ea34788ac", SIDECHAIN_ROOTSTOCK}
};


bool IsSidechainNumberValid(uint8_t nSidechain);

std::string GetSidechainName(uint8_t nSidechain);

#endif // BITCOIN_SIDECHAIN_H
