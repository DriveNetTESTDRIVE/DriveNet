// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAIN_H
#define BITCOIN_SIDECHAIN_H

#include <primitives/transaction.h>
#include <pubkey.h>

#include <array>

//! Max number of WT^(s) per sidechain during verification period
static const int SIDECHAIN_MAX_WT = 32; // TODO remove (CryptAxe wants this, psztorc does not)
static const size_t VALID_SIDECHAINS_COUNT = 2;

// These are the temporary values to speed things up during testing
static const int SIDECHAIN_VERIFICATION_PERIOD = 300;
static const int SIDECHAIN_MIN_WORKSCORE = 141;

// These are the values that will be used in the final release
//static const int SIDECHAIN_VERIFICATION_PERIOD = 26298;
//static const int SIDECHAIN_MIN_WORKSCORE = 13140;

//! Sidechain deposit fee (TODO make configurable per sidechain)
static const CAmount SIDECHAIN_DEPOSIT_FEE = 0.00001 * COIN;

enum SidechainNumber {
    SIDECHAIN_ONE = 0,
    SIDECHAIN_PAYMENT = 1,
};

// Configuration files
static const std::array<std::pair<std::string, std::string>, VALID_SIDECHAINS_COUNT + 1> ConfigDirectories =
{{
    {".drivenet", "drivenet.conf"},
    {".sidechain", "sidechain.conf"},
    {".payment", "payment.conf"},
}};

struct Sidechain {
    uint8_t nSidechain;
    const char* sidechainKey;
    const char* sidechainPriv;
    const char* sidechainHex;

    std::string GetSidechainName() const;
    bool operator==(const Sidechain& a) const;
    std::string ToString() const;
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
    //
    // {nSidechain, sidechain keyID, sidechain private key, public key}
    //
    // Sidechain One (test sidechain)
    {SIDECHAIN_ONE,
        "51c6eb4891cbb94ca30518b5f8441ea078c849eb",
        "L4nNEPEuYwNaKMj1RZVsAuXPq5xbhdio43dTRzAr5CZgQHrSpFU8",
        "76a91451c6eb4891cbb94ca30518b5f8441ea078c849eb88ac"},
    //
    // Payments sidechain
    {SIDECHAIN_PAYMENT,
        "7c33a3f6d9d5b873f96dba4b12d6aaf6be71fbd2",
        "L3UjtLhNXKZaDgFtf14EHkxV1p5CKUoyRUT5DcU7aUS1X2yX8hhg",
        "76a9147c33a3f6d9d5b873f96dba4b12d6aaf6be71fbd288ac"},
}};

static const std::map<std::string, int> ValidSidechainField =
{
    {"76a91451c6eb4891cbb94ca30518b5f8441ea078c849eb88ac", SIDECHAIN_ONE},
    {"76a9147c33a3f6d9d5b873f96dba4b12d6aaf6be71fbd288ac", SIDECHAIN_PAYMENT},
};


bool IsSidechainNumberValid(uint8_t nSidechain);

std::string GetSidechainName(uint8_t nSidechain);

#endif // BITCOIN_SIDECHAIN_H
