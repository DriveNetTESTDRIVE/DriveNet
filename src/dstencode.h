#ifndef BITCOIN_DSTENCODE_H
#define BITCOIN_DSTENCODE_H

// key.h and pubkey.h are not used here, but gcc doesn't want to instantiate
// CTxBCHDestination if types are unknown
#include <key.h>
#include <pubkey.h>
#include <script/standard.h>
#include <string>

class CChainParams;

std::string EncodeBCHDestination(const CTxBCHDestination &dest, bool fUseLegacy = false);

CTxBCHDestination DecodeBCHDestination(const std::string &addr, const CChainParams& params);

bool IsValidBCHDestinationString(const std::string &addr,
                              const CChainParams &params);

#endif // BITCOIN_DSTENCODE_H
