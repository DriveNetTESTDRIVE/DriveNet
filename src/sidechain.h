// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAIN_H
#define BITCOIN_SIDECHAIN_H

#include <primitives/transaction.h>
#include <pubkey.h>

#include <array>

//! Max number of cached WT^(s) during verification period
static const int SIDECHAIN_MAX_WT = 2; // TODO remove (CryptAxe wants this, psztorc does not)

// These are the temporary values to speed things up during testing
static const int SIDECHAIN_VERIFICATION_PERIOD = 300;
static const int SIDECHAIN_MIN_WORKSCORE = 141;

// These are the values that will be used in the final release
//static const int SIDECHAIN_VERIFICATION_PERIOD = 26298;
//static const int SIDECHAIN_MIN_WORKSCORE = 13140;

//! Sidechain deposit fee (TODO make configurable per sidechain)
static const CAmount SIDECHAIN_DEPOSIT_FEE = 0.00001 * COIN;

//! Maximum number of failures out of 2016 for a sidechain to activate
static const int SIDECHAIN_ACTIVATION_MAX_FAILURES = 40;
//! The amount of time a sidechain has to activate
static const int SIDECHAIN_ACTIVATION_MAX_AGE = 2016;
//! The number of sidechains which may be signaled for activation at once
static const int SIDECHAIN_ACTIVATION_MAX_SIGNALS = 32;
//! The number of sidechains which may be active at once
static const int SIDECHAIN_ACTIVATION_MAX_ACTIVE = 256;

// Configuration files
static const int CONF_COUNT = 3;
static const std::array<std::pair<std::string, std::string>, CONF_COUNT> ConfigDirectories =
{{
    // Note that these may or may not exist and will need to be updated as
    // sidechains are activated / deactivated for GUI purposes.
    // TODO come up with a smart way of automating this.
    {".drivenet", "drivenet.conf"},
    {".testchain", "testchain.conf"},
    {".paychain", "paychain.conf"},
}};

struct SidechainProposal {
    std::string title;
    std::string description;
    std::string sidechainKey;
    std::string sidechainHex;
    std::string sidechainPriv;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(title);
        READWRITE(description);
        READWRITE(sidechainKey);
        READWRITE(sidechainHex);
        READWRITE(sidechainPriv);
    }

    bool DeserializeFromScript(const CScript& script);

    std::vector<unsigned char> GetBytes() const;
    CScript GetScript() const;
    uint256 GetHash() const;
    bool operator==(const SidechainProposal& proposal) const;
    std::string ToString() const;
};

struct SidechainActivationStatus
{
    int nAge;
    int nFail;
    SidechainProposal proposal;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nAge);
        READWRITE(nFail);
        READWRITE(proposal);
    }
};

struct Sidechain {
    uint8_t nSidechain;
    std::string sidechainKey;
    std::string sidechainPriv;
    std::string sidechainHex;
    std::string title;
    std::string description;

    std::string GetSidechainName() const;
    bool operator==(const Sidechain& a) const;
    bool operator==(const SidechainProposal& a) const;
    std::string ToString() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(sidechainKey);
        READWRITE(sidechainPriv);
        READWRITE(sidechainHex);
        READWRITE(title);
        READWRITE(description);
    }
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CMutableTransaction tx;
    uint32_t n;
    uint256 hashBlock;

    bool operator==(const SidechainDeposit& a) const;
    std::string ToString() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSidechain);
        READWRITE(keyID);
        READWRITE(tx);
        READWRITE(n);
        READWRITE(hashBlock);
    }
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

// TODO refactor all of this for clarity of code
struct SCDBIndex {
    uint8_t nSidechain;
    std::array<SidechainWTPrimeState, SIDECHAIN_MAX_WT> members;
    bool IsPopulated() const;
    bool IsFull() const;
    bool InsertMember(const SidechainWTPrimeState& member);
    void ClearMembers();
    unsigned int CountPopulatedMembers() const;
    bool Contains(uint256 hashWT) const;
    bool GetMember(uint256 hashWT, SidechainWTPrimeState& wt) const;
};

#endif // BITCOIN_SIDECHAIN_H
