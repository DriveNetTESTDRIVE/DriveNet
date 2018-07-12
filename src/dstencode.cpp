// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <dstencode.h>
#include <base58.h>
#include <cashaddrenc.h>
#include <chainparams.h>
#include <script/standard.h>

#include <iostream>

std::string EncodeBCHDestination(const CTxBCHDestination &dest, bool fUseLegacy) {
    const CChainParams &params = Params();
    if (fUseLegacy)
        return EncodeLegacyAddr(dest, params);
    else
        return EncodeCashAddr(dest, params);
}

CTxBCHDestination DecodeBCHDestination(const std::string &addr,
                                 const CChainParams &params) {

    std::cout << "dstencode DecodeBCHDestination 0\n";

    CTxBCHDestination dst = DecodeCashAddr(addr, params);
    if (IsValidBCHDestination(dst)) {
    std::cout << "dstencode DecodeBCHDestination 1\n";
        return dst;
    }
    std::cout << "dstencode DecodeBCHDestination 2\n";
    return DecodeLegacyAddr(addr, params);
}

bool IsValidBCHDestinationString(const std::string &addr,
                              const CChainParams &params) {
    return IsValidBCHDestination(DecodeBCHDestination(addr, params));
}

namespace {
class DestinationEncoder : public boost::static_visitor<std::string> {
private:
    const CChainParams &m_params;

public:
    DestinationEncoder(const CChainParams &params) : m_params(params) {}

    std::string operator()(const CKeyID &id) const {
        std::vector<uint8_t> data =
            m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID &id) const {
        std::vector<uint8_t> data =
            m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination &no) const { return ""; }
};
}

