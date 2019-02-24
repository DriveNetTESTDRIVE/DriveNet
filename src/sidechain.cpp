// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidechain.h>

#include <clientversion.h>
#include <hash.h>
#include <script/script.h>
#include <streams.h>
#include <utilstrencodings.h>

#include <sstream>


bool Sidechain::operator==(const Sidechain& a) const
{
    return (a.nSidechain == nSidechain);
}

bool Sidechain::operator==(const SidechainProposal& a) const
{
    return (sidechainPriv == a.sidechainPriv &&
            sidechainHex == a.sidechainHex &&
            sidechainKeyID == a.sidechainKeyID &&
            title == a.title &&
            description == a.description &&
            hashID1 == a.hashID1 &&
            hashID2 == a.hashID2 &&
            nVersion == a.nVersion);
}

std::string Sidechain::GetSidechainName() const
{
    return title;
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nVersion=" << nVersion << std::endl;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "sidechainPriv=" << sidechainPriv << std::endl;
    ss << "sidechainHex=" << sidechainHex << std::endl;
    ss << "sidechainKeyID=" << sidechainKeyID << std::endl;
    ss << "title=" << title << std::endl;
    ss << "description=" << description << std::endl;
    ss << "hashID1=" << hashID1.ToString() << std::endl;
    ss << "hashID2=" << hashID2.ToString() << std::endl;
    return ss.str();
}

std::string SidechainProposal::ToString() const
{
    std::stringstream ss;
    ss << "sidechainPriv=" << sidechainPriv << std::endl;
    ss << "sidechainHex=" << sidechainHex << std::endl;
    ss << "sidechainKeyID=" << sidechainKeyID << std::endl;
    ss << "title=" << title << std::endl;
    ss << "description=" << description << std::endl;
    ss << "hashID1=" << hashID1.ToString() << std::endl;
    ss << "hashID2=" << hashID2.ToString() << std::endl;
    ss << "nVersion=" << nVersion << std::endl;
    return ss.str();
}

bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.nSidechain == nSidechain &&
            a.keyID == keyID &&
            a.tx == tx);
}

bool SidechainProposal::operator==(const SidechainProposal& proposal) const
{
    return (proposal.title == title &&
            proposal.description == description &&
            proposal.sidechainKeyID == sidechainKeyID &&
            proposal.sidechainHex == sidechainHex &&
            proposal.sidechainPriv == sidechainPriv &&
            proposal.hashID1 == hashID1 &&
            proposal.hashID2 == hashID2 &&
            proposal.nVersion == nVersion);
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "nsidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "keyid=" << keyID.ToString() << std::endl;
    ss << "txid=" << tx.GetHash().ToString() << std::endl;
    ss << "n=" << n << std::endl;
    ss << "hashblock=" << hashBlock.ToString() << std::endl;
    return ss.str();
}

bool SidechainLD::operator==(const SidechainLD& a) const
{
    return (a.nSidechain == nSidechain &&
            a.nPrevBlockRef == nPrevBlockRef &&
            a.hashCritical == hashCritical);
}

uint256 SidechainLD::GetHash(void) const
{
    return SerializeHash(*this);
}

bool SidechainWTPrimeState::IsNull() const
{
    return (hashWTPrime.IsNull());
}

uint256 SidechainWTPrimeState::GetHash(void) const
{
    return SerializeHash(*this);
}

bool SidechainWTPrimeState::operator==(const SidechainWTPrimeState& a) const
{
    return (a.nSidechain == nSidechain &&
            a.hashWTPrime == hashWTPrime);
}

std::string SidechainWTPrimeState::ToString() const
{
    std::stringstream ss;
    ss << "hash=" << GetHash().ToString() << std::endl;
    ss << "nsidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << (unsigned int)nBlocksLeft << std::endl;
    ss << "nWorkScore=" << (unsigned int)nWorkScore << std::endl;
    ss << "hashWTPrime=" << hashWTPrime.ToString() << std::endl;
    return ss.str();
}

bool SidechainProposal::DeserializeFromScript(const CScript& script)
{
    if (!script.IsSidechainProposalCommit())
        return false;

    CScript::const_iterator pc = script.begin() + 5;
    std::vector<unsigned char> vch;

    opcodetype opcode;
    if (!script.GetOp(pc, opcode, vch))
        return false;
    if (vch.empty())
        return false;

    const char *vch0 = (const char *) &vch.begin()[0];
    CDataStream ds(vch0, vch0+vch.size(), SER_DISK, CLIENT_VERSION);

    SidechainProposal sidechain;
    sidechain.Unserialize(ds);

    nVersion = sidechain.nVersion;
    title = sidechain.title;
    description = sidechain.description;
    sidechainKeyID = sidechain.sidechainKeyID;
    sidechainHex = sidechain.sidechainHex;
    sidechainPriv = sidechain.sidechainPriv;
    hashID1 = sidechain.hashID1;
    hashID2 = sidechain.hashID2;

    return true;
}

uint256 SidechainProposal::GetHash() const
{
    return SerializeHash(*(SidechainProposal *) this);
}

std::vector<unsigned char> SidechainProposal::GetBytes() const
{
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ((SidechainProposal *) this)->Serialize(ds);
    return std::vector<unsigned char>(ds.begin(), ds.end());
}

CScript SidechainProposal::GetScript() const
{
    CScript script;
    script.resize(5);
    script[0] = OP_RETURN;
    script[1] = 0xD5;
    script[2] = 0xE0;
    script[3] = 0xC4;
    script[4] = 0xAF;
    script << GetBytes();
    return script;
}
