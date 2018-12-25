// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <hash.h>
#include <sidechain.h>
#include <tinyformat.h>
#include <utilstrencodings.h>

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

uint256 COutPoint::GetHash() const
{
    return SerializeHash(*this);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), criticalData(tx.criticalData), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}
uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::GetWitnessHash() const
{
    if (!HasWitness()) {
        return GetHash();
    }
    return SerializeHash(*this, SER_GETHASH, 0);
}

bool CTransaction::GetBWTHash(uint256& hashRet) const
{
    CMutableTransaction mtx(*this);
    if (!mtx.vin.size() || !mtx.vout.size())
        return false;

    // Remove the CTIP scriptSig (set to OP_0 as the sidechain must orignally)
    mtx.vin[0].scriptSig = CScript() << OP_0;

    // Remove the sidechain change return
    mtx.vout.pop_back();

    // We now have the B-WT^ hash
    hashRet = mtx.GetHash();

    return true;
}

/* For backward compatibility, the hash is initialized to 0. TODO: remove the need for this default constructor entirely. */
CTransaction::CTransaction() : vin(), vout(), criticalData(), nVersion(CTransaction::CURRENT_VERSION), nLockTime(0), hash() {}
CTransaction::CTransaction(const CMutableTransaction &tx) : vin(tx.vin), vout(tx.vout), criticalData(tx.criticalData), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), criticalData(tx.criticalData), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash(ComputeHash()) {}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const auto& tx_out : vout) {
        nValueOut += tx_out.nValue;
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].scriptWitness.ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    if (!criticalData.IsNull()) {
        str += strprintf("Critical Data:\nbytes.size=%s\nhashCritical=%s",
        criticalData.bytes.size(),
        criticalData.hashCritical.ToString());
    }
    return str;
}

bool CCriticalData::IsBMMRequest() const
{
    uint8_t nSidechain;
    uint16_t nPrevBlockRef;

    return IsBMMRequest(nSidechain, nPrevBlockRef);
}

bool CCriticalData::IsBMMRequest(uint8_t& nSidechain, uint16_t& nPrevBlockRef) const
{
    // Check for h* commit flag in critical data bytes
    if (IsNull())
        return false;
    if (bytes.size() < 4)
        return false;

    if (bytes[0] != 0x00 || bytes[1] != 0xbf || bytes[2] != 0x00)
        return false;

    if (bytes.size() == 4) {
        nSidechain = 0;
        // TODO re-enable? By doing some refactoring we could re-enable this
        // check here. However, I think it would be better to just do the check
        // someone else outside of this class.
        //if (!IsSidechainNumberValid(nSidechain))
        //    return false;
        //

        std::vector<unsigned char> vch;
        vch.push_back(bytes[3]);
        nPrevBlockRef = CScriptNum(vch, false).getint();
    } else {
        std::vector<unsigned char> vch;
        vch.push_back(bytes[3]);

        nSidechain = CScriptNum(vch, false).getint();
        // TODO re-enable?
        //if (!IsSidechainNumberValid(nSidechain))
        //    return false;

        std::vector<unsigned char> vchPrev;
        vchPrev.push_back(bytes[4]);
        nPrevBlockRef = CScriptNum(vchPrev, false).getint();
    }

    return true;
}
