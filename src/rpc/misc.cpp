// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <chain.h>
#include <clientversion.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <crypto/ripemd160.h>
#include <httpserver.h>
#include <init.h>
#include <merkleblock.h>
#include <net.h>
#include <netbase.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <sidechain.h>
#include <sidechaindb.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <utilstrencodings.h>
#include <validation.h>
#ifdef ENABLE_WALLET
#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#endif
#include <warnings.h>

#include <stdint.h>
#ifdef HAVE_MALLOC_INFO
#include <malloc.h>
#endif

#include <univalue.h>

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    CWallet * const pwallet;

    explicit DescribeAddressVisitor(CWallet *_pwallet) : pwallet(_pwallet) {}

    void ProcessSubScript(const CScript& subscript, UniValue& obj, bool include_addresses = false) const
    {
        // Always present: script type and redeemscript
        txnouttype which_type;
        std::vector<std::vector<unsigned char>> solutions_data;
        Solver(subscript, which_type, solutions_data);
        obj.pushKV("script", GetTxnOutputType(which_type));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));

        CTxDestination embedded;
        UniValue a(UniValue::VARR);
        if (ExtractDestination(subscript, embedded)) {
            // Only when the script corresponds to an address.
            UniValue subobj = boost::apply_visitor(*this, embedded);
            subobj.pushKV("address", EncodeDestination(embedded));
            subobj.pushKV("scriptPubKey", HexStr(subscript.begin(), subscript.end()));
            // Always report the pubkey at the top level, so that `getnewaddress()['pubkey']` always works.
            if (subobj.exists("pubkey")) obj.pushKV("pubkey", subobj["pubkey"]);
            obj.pushKV("embedded", std::move(subobj));
            if (include_addresses) a.push_back(EncodeDestination(embedded));
        } else if (which_type == TX_MULTISIG) {
            // Also report some information on multisig scripts (which do not have a corresponding address).
            // TODO: abstract out the common functionality between this logic and ExtractDestinations.
            obj.pushKV("sigsrequired", solutions_data[0][0]);
            UniValue pubkeys(UniValue::VARR);
            for (size_t i = 1; i < solutions_data.size() - 1; ++i) {
                CPubKey key(solutions_data[i].begin(), solutions_data[i].end());
                if (include_addresses) a.push_back(EncodeDestination(key.GetID()));
                pubkeys.push_back(HexStr(key.begin(), key.end()));
            }
            obj.pushKV("pubkeys", std::move(pubkeys));
        }

        // The "addresses" field is confusing because it refers to public keys using their P2PKH address.
        // For that reason, only add the 'addresses' field when needed for backward compatibility. New applications
        // can use the 'embedded'->'address' field for P2SH or P2WSH wrapped addresses, and 'pubkeys' for
        // inspecting multisig participants.
        if (include_addresses) obj.pushKV("addresses", std::move(a));
    }

    UniValue operator()(const CNoDestination &dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID &keyID) const {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", false));
        if (pwallet && pwallet->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", false));
        if (pwallet && pwallet->GetCScript(scriptID, subscript)) {
            ProcessSubScript(subscript, obj, true);
        }
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey pubkey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        if (pwallet && pwallet->GetPubKey(CKeyID(id), pubkey)) {
            obj.push_back(Pair("pubkey", HexStr(pubkey)));
        }
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        CRIPEMD160 hasher;
        uint160 hash;
        hasher.Write(id.begin(), 32).Finalize(hash.begin());
        if (pwallet && pwallet->GetCScript(CScriptID(hash), subscript)) {
            ProcessSubScript(subscript, obj);
        }
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", (int)id.version));
        obj.push_back(Pair("witness_program", HexStr(id.program, id.program + id.length)));
        return obj;
    }
};
#endif

UniValue validateaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "validateaddress \"address\"\n"
            "\nReturn information about the given bitcoin address.\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The bitcoin address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,       (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"address\",        (string) The bitcoin address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,        (boolean) If the address is yours or not\n"
            "  \"iswatchonly\" : true|false,   (boolean) If the address is watchonly\n"
            "  \"isscript\" : true|false,      (boolean, optional) If the address is P2SH or P2WSH. Not included for unknown witness types.\n"
            "  \"iswitness\" : true|false,     (boolean) If the address is P2WPKH, P2WSH, or an unknown witness version\n"
            "  \"witness_version\" : version   (number, optional) For all witness output types, gives the version number.\n"
            "  \"witness_program\" : \"hex\"     (string, optional) For all witness output types, gives the script or key hash present in the address.\n"
            "  \"script\" : \"type\"             (string, optional) The output script type. Only if \"isscript\" is true and the redeemscript is known. Possible types: nonstandard, pubkey, pubkeyhash, scripthash, multisig, nulldata, witness_v0_keyhash, witness_v0_scripthash, witness_unknown\n"
            "  \"hex\" : \"hex\",                (string, optional) The redeemscript for the P2SH or P2WSH address\n"
            "  \"addresses\"                   (string, optional) Array of addresses associated with the known redeemscript (only if \"iswitness\" is false). This field is superseded by the \"pubkeys\" field and the address inside \"embedded\".\n"
            "    [\n"
            "      \"address\"\n"
            "      ,...\n"
            "    ]\n"
            "  \"pubkeys\"                     (string, optional) Array of pubkeys associated with the known redeemscript (only if \"script\" is \"multisig\")\n"
            "    [\n"
            "      \"pubkey\"\n"
            "      ,...\n"
            "    ]\n"
            "  \"sigsrequired\" : xxxxx        (numeric, optional) Number of signatures required to spend multisig output (only if \"script\" is \"multisig\")\n"
            "  \"pubkey\" : \"publickeyhex\",    (string, optional) The hex value of the raw public key, for single-key addresses (possibly embedded in P2SH or P2WSH)\n"
            "  \"embedded\" : {...},           (object, optional) information about the address embedded in P2SH or P2WSH, if relevant and known. It includes all validateaddress output fields for the embedded address, excluding \"isvalid\", metadata (\"timestamp\", \"hdkeypath\", \"hdmasterkeyid\") and relation to the wallet (\"ismine\", \"iswatchonly\", \"account\").\n"
            "  \"iscompressed\" : true|false,  (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"         (string) DEPRECATED. The account associated with the address, \"\" is the default account\n"
            "  \"timestamp\" : timestamp,      (number, optional) The creation time of the key if available in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"hdkeypath\" : \"keypath\"       (string, optional) The HD keypath if the key is HD and available\n"
            "  \"hdmasterkeyid\" : \"<hash160>\" (string, optional) The Hash160 of the HD master pubkey\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );

#ifdef ENABLE_WALLET
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : nullptr);
#else
    LOCK(cs_main);
#endif

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        std::string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", currentAddress));

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

#ifdef ENABLE_WALLET
        isminetype mine = pwallet ? IsMine(*pwallet, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", bool(mine & ISMINE_SPENDABLE)));
        ret.push_back(Pair("iswatchonly", bool(mine & ISMINE_WATCH_ONLY)));
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(pwallet), dest);
        ret.pushKVs(detail);
        if (pwallet && pwallet->mapAddressBook.count(dest)) {
            ret.push_back(Pair("account", pwallet->mapAddressBook[dest].name));
        }
        if (pwallet) {
            const CKeyMetadata* meta = nullptr;
            CKeyID key_id = GetKeyForDestination(*pwallet, dest);
            if (!key_id.IsNull()) {
                auto it = pwallet->mapKeyMetadata.find(key_id);
                if (it != pwallet->mapKeyMetadata.end()) {
                    meta = &it->second;
                }
            }
            if (!meta) {
                auto it = pwallet->m_script_metadata.find(CScriptID(scriptPubKey));
                if (it != pwallet->m_script_metadata.end()) {
                    meta = &it->second;
                }
            }
            if (meta) {
                ret.push_back(Pair("timestamp", meta->nCreateTime));
                if (!meta->hdKeypath.empty()) {
                    ret.push_back(Pair("hdkeypath", meta->hdKeypath));
                    ret.push_back(Pair("hdmasterkeyid", meta->hdMasterKeyID.GetHex()));
                }
            }
        }
#endif
    }
    return ret;
}

// Needed even with !ENABLE_WALLET, to pass (ignored) pointers around
class CWallet;

UniValue createmultisig(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 2)
    {
        std::string msg = "createmultisig nrequired [\"key\",...]\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"
            "\nArguments:\n"
            "1. nrequired                    (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"                       (string, required) A json array of hex-encoded public keys\n"
            "     [\n"
            "       \"key\"                    (string) The hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 public keys\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"")
        ;
        throw std::runtime_error(msg);
    }

    int required = request.params[0].get_int();

    // Get the public keys
    const UniValue& keys = request.params[1].get_array();
    std::vector<CPubKey> pubkeys;
    for (unsigned int i = 0; i < keys.size(); ++i) {
        if (IsHex(keys[i].get_str()) && (keys[i].get_str().length() == 66 || keys[i].get_str().length() == 130)) {
            pubkeys.push_back(HexToPubKey(keys[i].get_str()));
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Invalid public key: %s\nNote that from v0.16, createmultisig no longer accepts addresses."
            " Users must use addmultisigaddress to create multisig addresses with addresses known to the wallet.", keys[i].get_str()));
        }
    }

    // Construct using pay-to-script-hash:
    CScript inner = CreateMultisigRedeemscript(required, pubkeys);
    CScriptID innerID(inner);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(innerID)));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

UniValue verifymessage(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "verifymessage \"address\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The bitcoin address to use for the signature.\n"
            "2. \"signature\"       (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    std::string strAddress  = request.params[0].get_str();
    std::string strSign     = request.params[1].get_str();
    std::string strMessage  = request.params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

UniValue signmessagewithprivkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "signmessagewithprivkey \"privkey\" \"message\"\n"
            "\nSign a message with the private key of an address\n"
            "\nArguments:\n"
            "1. \"privkey\"         (string, required) The private key to sign the message with.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nCreate the signature\n"
            + HelpExampleCli("signmessagewithprivkey", "\"privkey\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        );

    std::string strPrivkey = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strPrivkey);
    if (!fGood)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    CKey key = vchSecret.GetKey();
    if (!key.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

UniValue setmocktime(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time."
        );

    if (!Params().MineBlocksOnDemand())
        throw std::runtime_error("setmocktime for regression testing (-regtest mode) only");

    // For now, don't change mocktime if we're in the middle of validation, as
    // this could have an effect on mempool time-based eviction, as well as
    // IsCurrentForFeeEstimation() and IsInitialBlockDownload().
    // TODO: figure out the right way to synchronize around mocktime, and
    // ensure all call sites of GetTime() are accessing this safely.
    LOCK(cs_main);

    RPCTypeCheck(request.params, {UniValue::VNUM});
    SetMockTime(request.params[0].get_int64());

    return NullUniValue;
}

static UniValue RPCLockedMemoryInfo()
{
    LockedPool::Stats stats = LockedPoolManager::Instance().stats();
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("used", uint64_t(stats.used)));
    obj.push_back(Pair("free", uint64_t(stats.free)));
    obj.push_back(Pair("total", uint64_t(stats.total)));
    obj.push_back(Pair("locked", uint64_t(stats.locked)));
    obj.push_back(Pair("chunks_used", uint64_t(stats.chunks_used)));
    obj.push_back(Pair("chunks_free", uint64_t(stats.chunks_free)));
    return obj;
}

#ifdef HAVE_MALLOC_INFO
static std::string RPCMallocInfo()
{
    char *ptr = nullptr;
    size_t size = 0;
    FILE *f = open_memstream(&ptr, &size);
    if (f) {
        malloc_info(0, f);
        fclose(f);
        if (ptr) {
            std::string rv(ptr, size);
            free(ptr);
            return rv;
        }
    }
    return "";
}
#endif

UniValue getmemoryinfo(const JSONRPCRequest& request)
{
    /* Please, avoid using the word "pool" here in the RPC interface or help,
     * as users will undoubtedly confuse it with the other "memory pool"
     */
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getmemoryinfo (\"mode\")\n"
            "Returns an object containing information about memory usage.\n"
            "Arguments:\n"
            "1. \"mode\" determines what kind of information is returned. This argument is optional, the default mode is \"stats\".\n"
            "  - \"stats\" returns general statistics about memory usage in the daemon.\n"
            "  - \"mallocinfo\" returns an XML string describing low-level heap state (only available if compiled with glibc 2.10+).\n"
            "\nResult (mode \"stats\"):\n"
            "{\n"
            "  \"locked\": {               (json object) Information about locked memory manager\n"
            "    \"used\": xxxxx,          (numeric) Number of bytes used\n"
            "    \"free\": xxxxx,          (numeric) Number of bytes available in current arenas\n"
            "    \"total\": xxxxxxx,       (numeric) Total number of bytes managed\n"
            "    \"locked\": xxxxxx,       (numeric) Amount of bytes that succeeded locking. If this number is smaller than total, locking pages failed at some point and key data could be swapped to disk.\n"
            "    \"chunks_used\": xxxxx,   (numeric) Number allocated chunks\n"
            "    \"chunks_free\": xxxxx,   (numeric) Number unused chunks\n"
            "  }\n"
            "}\n"
            "\nResult (mode \"mallocinfo\"):\n"
            "\"<malloc version=\"1\">...\"\n"
            "\nExamples:\n"
            + HelpExampleCli("getmemoryinfo", "")
            + HelpExampleRpc("getmemoryinfo", "")
        );

    std::string mode = request.params[0].isNull() ? "stats" : request.params[0].get_str();
    if (mode == "stats") {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("locked", RPCLockedMemoryInfo()));
        return obj;
    } else if (mode == "mallocinfo") {
#ifdef HAVE_MALLOC_INFO
        return RPCMallocInfo();
#else
        throw JSONRPCError(RPC_INVALID_PARAMETER, "mallocinfo is only available when compiled with glibc 2.10+");
#endif
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown mode " + mode);
    }
}

uint32_t getCategoryMask(UniValue cats) {
    cats = cats.get_array();
    uint32_t mask = 0;
    for (unsigned int i = 0; i < cats.size(); ++i) {
        uint32_t flag = 0;
        std::string cat = cats[i].get_str();
        if (!GetLogCategory(&flag, &cat)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown logging category " + cat);
        }
        if (flag == BCLog::NONE) {
            return 0;
        }
        mask |= flag;
    }
    return mask;
}

UniValue logging(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "logging ( <include> <exclude> )\n"
            "Gets and sets the logging configuration.\n"
            "When called without an argument, returns the list of categories with status that are currently being debug logged or not.\n"
            "When called with arguments, adds or removes categories from debug logging and return the lists above.\n"
            "The arguments are evaluated in order \"include\", \"exclude\".\n"
            "If an item is both included and excluded, it will thus end up being excluded.\n"
            "The valid logging categories are: " + ListLogCategories() + "\n"
            "In addition, the following are available as category names with special meanings:\n"
            "  - \"all\",  \"1\" : represent all logging categories.\n"
            "  - \"none\", \"0\" : even if other logging categories are specified, ignore all of them.\n"
            "\nArguments:\n"
            "1. \"include\"        (array of strings, optional) A json array of categories to add debug logging\n"
            "     [\n"
            "       \"category\"   (string) the valid logging category\n"
            "       ,...\n"
            "     ]\n"
            "2. \"exclude\"        (array of strings, optional) A json array of categories to remove debug logging\n"
            "     [\n"
            "       \"category\"   (string) the valid logging category\n"
            "       ,...\n"
            "     ]\n"
            "\nResult:\n"
            "{                   (json object where keys are the logging categories, and values indicates its status\n"
            "  \"category\": 0|1,  (numeric) if being debug logged or not. 0:inactive, 1:active\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("logging", "\"[\\\"all\\\"]\" \"[\\\"http\\\"]\"")
            + HelpExampleRpc("logging", "[\"all\"], \"[libevent]\"")
        );
    }

    uint32_t originalLogCategories = logCategories;
    if (request.params[0].isArray()) {
        logCategories |= getCategoryMask(request.params[0]);
    }

    if (request.params[1].isArray()) {
        logCategories &= ~getCategoryMask(request.params[1]);
    }

    // Update libevent logging if BCLog::LIBEVENT has changed.
    // If the library version doesn't allow it, UpdateHTTPServerLogging() returns false,
    // in which case we should clear the BCLog::LIBEVENT flag.
    // Throw an error if the user has explicitly asked to change only the libevent
    // flag and it failed.
    uint32_t changedLogCategories = originalLogCategories ^ logCategories;
    if (changedLogCategories & BCLog::LIBEVENT) {
        if (!UpdateHTTPServerLogging(logCategories & BCLog::LIBEVENT)) {
            logCategories &= ~BCLog::LIBEVENT;
            if (changedLogCategories == BCLog::LIBEVENT) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "libevent logging cannot be updated when using libevent before v2.1.1.");
            }
        }
    }

    UniValue result(UniValue::VOBJ);
    std::vector<CLogCategoryActive> vLogCatActive = ListActiveLogCategories();
    for (const auto& logCatActive : vLogCatActive) {
        result.pushKV(logCatActive.category, logCatActive.active);
    }

    return result;
}

UniValue createcriticaldatatx(const JSONRPCRequest& request)
{
    // TODO finish
    //
    if (request.fHelp || request.params.size() != 4)
        throw std::runtime_error(
            "createcriticaldatatx\n"
            "Create a critical data transaction\n"
            "\nArguments:\n"
            "1. \"amount\"         (numeric or string, required) The amount in " + CURRENCY_UNIT + " to be spent.\n"
            "2. \"height\"         (numeric, required) The block height this transaction must be included in.\n"
            "3. \"criticalhash\"   (string, required) h* you want added to a coinbase\n"
            "\nExamples:\n"
            + HelpExampleCli("createcriticaldatatx", "\"amount\", \"height\", \"criticalhash\"")
            + HelpExampleRpc("createcriticaldatatx", "\"amount\", \"height\", \"criticalhash\"")
            );

    // Amount
    CAmount nAmount = AmountFromValue(request.params[0]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

    int nHeight = request.params[1].get_int();

    // Critical hash
    uint256 hashCritical = uint256S(request.params[2].get_str());
    if (hashCritical.IsNull())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid h*");

#ifdef ENABLE_WALLET
    // Create and send the transaction
    std::string strError;
    if (vpwallets.empty()){
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    std::vector<CRecipient> vecSend;
    CRecipient recipient = {CScript() << OP_0, nAmount, false};
    vecSend.push_back(recipient);

    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    CWalletTx wtx;
    CReserveKey reservekey(vpwallets[0]);
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    //TODO: set this as a real thing
    CCoinControl cc;
    if (!vpwallets[0]->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, cc)) {
        if (nAmount + nFeeRequired > vpwallets[0]->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CValidationState state;
    if (!vpwallets[0]->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
#endif

    UniValue ret(UniValue::VOBJ);
#ifdef ENABLE_WALLET
    ret.push_back(Pair("txid", wtx.GetHash().GetHex()));
    ret.push_back(Pair("nChangePos", nChangePosRet));
#endif

    return ret;
}

UniValue createbmmcriticaldatatx(const JSONRPCRequest& request)
{
    // TODO handle optional height better
    if (request.fHelp || request.params.size() != 5)
        throw std::runtime_error(
            "createbmmcriticaldatatx\n"
            "Create a BMM request critical data transaction\n"
            "\nArguments:\n"
            "1. \"amount\"         (numeric or string, required) The amount in " + CURRENCY_UNIT + " to be spent.\n"
            "2. \"height\"         (numeric, required) The block height this transaction must be included in.\n"
            "Note: If 0 is passed in for height, current block height will be used"
            "3. \"criticalhash\"   (string, required) h* you want added to a coinbase\n"
            "4. \"nsidechain\"     (numeric, required) Sidechain requesting BMM\n"
            "5. \"ndag\"           (numeric, required) DAG number\n"
            "\nExamples:\n"
            + HelpExampleCli("createbmmcriticaldatatx", "\"amount\", \"height\", \"criticalhash\", \"nsidechain\", \"ndag\"")
            + HelpExampleRpc("createbmmcriticaldatatx", "\"amount\", \"height\", \"criticalhash\", \"nsidechain\", \"ndag\"")
            );

    // Amount
    CAmount nAmount = AmountFromValue(request.params[0]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

    // Height
    int nHeight = request.params[1].get_int();
    if (nHeight == 0) {
        LOCK(cs_main);
        nHeight = chainActive.Height();
    }

    // Critical hash
    uint256 hashCritical = uint256S(request.params[2].get_str());
    if (hashCritical.IsNull())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid h*");

    // nSidechain
    int nSidechain = request.params[3].get_int();

    if (!IsSidechainNumberValid(nSidechain))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid Sidechain number");

    // nDAG
    int nDAG = request.params[4].get_int();

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    bytes << CScriptNum(nSidechain);
    bytes << CScriptNum(nDAG);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = hashCritical;

#ifdef ENABLE_WALLET
    // Create and send the transaction
    std::string strError;
    if (vpwallets.empty()){
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Create transaction with critical data
    std::vector<CRecipient> vecSend;
    CRecipient recipient = {CScript() << OP_TRUE, nAmount, true};
    vecSend.push_back(recipient);

    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    CWalletTx wtx;
    CReserveKey reservekey(vpwallets[0]);
    CAmount nFeeRequired;
    int nChangePosRet = -1;
    //TODO: set this as a real thing
    CCoinControl cc;
    if (!vpwallets[0]->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, cc, true, 3, nHeight, criticalData)) {
        if (nAmount + nFeeRequired > vpwallets[0]->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CValidationState state;
    if (!vpwallets[0]->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
#endif

    UniValue ret(UniValue::VOBJ);
#ifdef ENABLE_WALLET
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", wtx.GetHash().ToString()));
    ret.push_back(Pair("txid", obj));
#endif

    return ret;
}

UniValue listsidechainctip(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "listsidechainctip\n"
            "Returns the crtitical transaction index pair for nSidechain\n"
            "\nArguments:\n"
            "1. \"nsidechain\"      (numeric, required) The sidechain number\n"
            "\nExamples:\n"
            + HelpExampleCli("listsidechainctip", "\"nsidechain\"")
            + HelpExampleRpc("listsidechainctip", "\"nsidechain\"")
            );

#ifdef ENABLE_WALLET
    // Check for active wallet
    std::string strError;
    if (vpwallets.empty()) {
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Is nSidechain valid?
    uint8_t nSidechain = (unsigned int)request.params[0].get_int();
    if (!IsSidechainNumberValid(nSidechain))
        throw std::runtime_error("Invalid sidechain number");

    CScript scriptPubKey;
    if (!scdb.GetSidechainScript(nSidechain, scriptPubKey))
        throw std::runtime_error("Invalid sidechain");

    std::vector<COutput> vSidechainCoin;
    vpwallets[0]->AvailableSidechainCoins(scriptPubKey, nSidechain, vSidechainCoin);
#endif

    if (vSidechainCoin.empty())
        throw std::runtime_error("No Sidechain CTIP found");

    if (vSidechainCoin.size() != 1)
        throw std::runtime_error("Invalid Sidechain CTIP (multiple CTIP found)");

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", vSidechainCoin.front().tx->GetHash().ToString()));
    obj.push_back(Pair("n", vSidechainCoin.front().i));

    return obj;
}

UniValue listsidechaindeposits(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "listsidechaindeposits\n"
            "List the most recent cached deposits (for nSidechain). Optionally "
            "limited to count. Note that this does not return all sidechain "
            "deposits, just the most recent deposits in the cache.\n"
            "\nArguments:\n"
            "1. \"hash\"    (string, required) The sidechain build commit hash\n"
            "2. \"count\"   (numeric, optional) The number of most recent deposits to list\n"
            "\nExamples:\n"
            + HelpExampleCli("listsidechaindeposits", "\"hash\", \"count\"")
            + HelpExampleRpc("listsidechaindeposits", "\"hash\", \"count\"")
            );

#ifdef ENABLE_WALLET
    // Check for active wallet
    std::string strError;
    if (vpwallets.empty()) {
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
#endif

    // Is sidechain build commit hash valid?
    std::string strSidechain = request.params[0].get_str();
    uint256 hashSidechain = uint256S(strSidechain);
    if (hashSidechain.IsNull())
        throw std::runtime_error("Invalid sidechain build commit hash!");

    // Figure out the base58 encoding of the private key
    CKey key;
    key.Set(hashSidechain.begin(), hashSidechain.end(), false);
    CBitcoinSecret vchSecret(key);

    // Get number of recent deposits to return (default is all cached deposits)
    bool fLimit = false;
    int count = 0;
    if (request.params.size() == 2) {
        fLimit = true;
        count = request.params[1].get_int();
    }

    UniValue arr(UniValue::VARR);

#ifdef ENABLE_WALLET
    std::vector<SidechainDeposit> vDeposit = scdb.GetDeposits(uint256S(vchSecret.ToString()));
    if (!vDeposit.size())
        throw std::runtime_error("No deposits in cache");

    for (auto rit = vDeposit.crbegin(); rit != vDeposit.crend(); rit++) {
        const SidechainDeposit d = *rit;

        // Add deposit txid to set
        uint256 txid = d.tx.GetHash();
        std::set<uint256> setTxids;
        setTxids.insert(txid);

        LOCK(cs_main);

        // TODO improve all of these error messages

        BlockMap::iterator it = mapBlockIndex.find(d.hashBlock);
        if (it == mapBlockIndex.end())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");

        CBlockIndex* pblockindex = it->second;
        if (pblockindex == NULL)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Block index null");

        if (!chainActive.Contains(pblockindex))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not in active chain");

        // Read block containing deposit output
        CBlock block;
        if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

        // Look for deposit transaction
        bool found = false;
        for (const auto& tx : block.vtx)
            if (tx->GetHash() == txid)
                found = true;
        if (!found)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "transaction not found in specified block");

        // Serialize and take hex of txout proof
        CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
        CMerkleBlock mb(block, setTxids);
        ssMB << mb;
        std::string strProofHex = HexStr(ssMB.begin(), ssMB.end());
#endif

#ifdef ENABLE_WALLET
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("nsidechain", d.nSidechain));
        obj.push_back(Pair("keyid", d.keyID.ToString()));
        obj.push_back(Pair("txhex", EncodeHexTx(d.tx)));
        obj.push_back(Pair("n", (int64_t)d.n));
        obj.push_back(Pair("proofhex", strProofHex));

        arr.push_back(obj);
#endif
        if (fLimit) {
            count--;
            if (count <= 0)
                break;
        }
    }

    return arr;
}

UniValue countsidechaindeposits(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "countsidechaindeposits\n"
            "Returns the number of deposits (for nSidechain) currently cached. "
            "Note that this doesn't count all sidechain deposits, just the "
            "number currently cached by the node.\n"
            "\nArguments:\n"
            "1. \"nsidechain\"      (numeric, required) The sidechain number\n"
            "\nExamples:\n"
            + HelpExampleCli("countsidechaindeposits", "\"nsidechain\"")
            + HelpExampleRpc("countsidechaindeposits", "\"nsidechain\"")
            );

#ifdef ENABLE_WALLET
    // Check for active wallet
    std::string strError;
    if (vpwallets.empty()) {
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
#endif

    // Is nSidechain valid?
    uint8_t nSidechain = std::stoi(request.params[0].getValStr());
    if (!IsSidechainNumberValid(nSidechain))
        throw std::runtime_error("Invalid sidechain number");

    int count = 0;

    // Get latest deposit from sidechain DB deposit cache
    std::vector<SidechainDeposit> vDeposit = scdb.GetDeposits(nSidechain);
    count = vDeposit.size();

    return count;
}

UniValue receivewtprime(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "receivewtprime\n"
            "Called by sidechain to announce new WT^ for verification\n"
            "\nArguments:\n"
            "1. \"nsidechain\"      (int, required) The sidechain number\n"
            "2. \"rawtx\"           (string, required) The raw transaction hex\n"
            "\nExamples:\n"
            + HelpExampleCli("receivewtprime", "")
            + HelpExampleRpc("receivewtprime", "")
     );

#ifdef ENABLE_WALLET
    // Check for active wallet
    std::string strError;
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) {
        strError = "Error: no wallets are available";
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    LOCK2(cs_main, &pwallet->cs_wallet);
#endif

    // Is nSidechain valid?
    int nSidechain = request.params[0].get_int();
    if (!IsSidechainNumberValid(nSidechain))
        throw std::runtime_error("Invalid sidechain number!");

    // Create CTransaction from hex
    CMutableTransaction mtx;
    std::string hex = request.params[1].get_str();
    if (!DecodeHexTx(mtx, hex))
        throw std::runtime_error("Invalid transaction hex!");

    CTransaction wtPrime(mtx);

    if (wtPrime.IsNull())
        throw std::runtime_error("Invalid WT^ hex");

#ifdef ENABLE_WALLET
    // Reject the WT^ if it spends more than the sidechain's CTIP as it won't
    // be accepted anyway
    CAmount amount = wtPrime.GetValueOut();
    std::vector<COutput> vSidechainCoin;
    CScript scriptPubKey;
    if (!scdb.GetSidechainScript(nSidechain, scriptPubKey))
        throw std::runtime_error("Invalid sidechain!");
    pwallet->AvailableSidechainCoins(scriptPubKey, nSidechain, vSidechainCoin);

    if (vSidechainCoin.empty())
        throw std::runtime_error("Rejecting WT^: No Sidechain CTIP found!");

    if (vSidechainCoin.size() != 1)
        throw std::runtime_error("Rejecting WT^: Invalid Sidechain CTIP (multiple CTIP found!)");

    if (amount > vSidechainCoin.front().tx->GetAvailableWatchOnlyCredit())
        throw std::runtime_error("Rejecting WT^: Withdrawn amount greater than CTIP amount!");
#endif

    // Add WT^ to our local cache so that we can create a WT^ hash commitment
    // in the next block we mine to begin the verification process
    if (!scdb.CacheWTPrime(wtPrime))
        throw std::runtime_error("WT^ rejected (duplicate?)");

    // Return WT^ hash to verify it has been received
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("wtxid", wtPrime.GetHash().GetHex()));
    return ret;
}

UniValue receivewtprimeupdate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "receivewtprimeupdate\n"
            "Receive an update for a WT^\n"
            "\nArguments:\n"
            "1. \"height\"                      (numeric, required) the block height\n"
            "2. \"updates\"                     (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"sidechainnumber\":n,    (numeric, required) The sidechain number\n"
            "         \"hashWTPrime\":id,       (string,  required) The WT^ hash\n"
            "         \"workscore\":n           (numeric, required) The updated workscore\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "\nExamples:\n"
            + HelpExampleCli("receivewtprimeupdate", "")
            + HelpExampleRpc("receivewtprimeupdate", "")
     );

    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VARR}, true);
    if (request.params[0].isNull() || request.params[1].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1 and 2 must be non-null");

    int nHeight = request.params[0].get_int();
    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = nHeight;

    UniValue inputs = request.params[1].get_array();
    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        // Get sidechain number
        const UniValue& sidechainnumber_v = find_value(o, "sidechainnumber");
        if (!sidechainnumber_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing sidechain number");
        uint8_t nSidechain = sidechainnumber_v.get_int();

        // Is nSidechain valid?
        if (!IsSidechainNumberValid(nSidechain))
            throw std::runtime_error("Invalid sidechain number");

        // Get WT^ hash
        uint256 hashWTPrime = ParseHashO(o, "hashWTPrime");
        if (hashWTPrime.IsNull())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing WT^ hash");

        // Get updated work score
        const UniValue& workscore_v = find_value(o, "workscore");
        if (!workscore_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing updated workscore");
        uint16_t nWorkScore = workscore_v.get_int();

        // create MSG
        SidechainUpdateMSG update;
        update.nSidechain = nSidechain;
        update.hashWTPrime = hashWTPrime;
        update.nWorkScore = nWorkScore;

        // add to package
        updatePackage.vUpdate.push_back(update);
    }

    // Add created package to SCDB WT^ update cache
    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    return true;
}

UniValue getbmmproof(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "getbmmproof\n"
            "Get the BMM proof (txoutproof) of an h* BMM commit transaction "
            "on the mainchain. Used by the sidechain (optionally) to double "
            "check BMM commits before connecting a sidechain block\n"
            "\nArguments:\n"
            "1. \"blockhash\"      (string, required) mainchain blockhash with h*\n"
            "2. \"criticalhash\"   (string, required) h* to create proof of\n"
            "\nExamples:\n"
            + HelpExampleCli("getbmmproof", "\"blockhash\", \"criticalhash\"")
            + HelpExampleRpc("getbmmproof", "\"blockhash\", \"criticalhash\"")
            );

    uint256 hashBlock = uint256S(request.params[0].get_str());
    uint256 hashCritical = uint256S(request.params[1].get_str());

    if (!mapBlockIndex.count(hashBlock))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hashBlock];
    if (pblockindex == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "pblockindex null");

    CBlock block;
    if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to read block from disk");

    if (!block.vtx.size())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No txns in block");

    bool fCriticalHashFound = false;
    const CTransaction &txCoinbase = *(block.vtx[0]);
    for (const CTxOut& out : txCoinbase.vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        if (scriptPubKey.size() < sizeof(uint256) + 6)
            continue;
        if (scriptPubKey[0] != OP_RETURN)
            continue;

        // Get h*
        std::vector<unsigned char> vch (scriptPubKey.begin() + 6, scriptPubKey.begin() + 38);

        // TODO return the bytes
        // Get Bytes
        if (scriptPubKey.size() > 38) {
            std::vector<unsigned char> vchBytes(scriptPubKey.begin() + 38, scriptPubKey.end());
        }

        if (hashCritical == uint256(vch))
            fCriticalHashFound = true;
    }

    if (!fCriticalHashFound)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "H* not found in block");

    std::string strProof = "";
    if (!GetTxOutProof(txCoinbase.GetHash(), hashBlock, strProof))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Could not get txoutproof...");

    std::string strCoinbaseHex = EncodeHexTx(txCoinbase);

    UniValue ret(UniValue::VOBJ);
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("proof", strProof));
    obj.push_back(Pair("coinbasehex", strCoinbaseHex));
    ret.push_back(Pair("proof", obj));

    return ret;
}

UniValue listpreviousblockhashes(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listpreviousblockhashes\n"
            "List the 5 most recent mainchain block hashes. Used by sidechains " \
            "to help search for BMM commitments.\n"
            "\nArguments:\n"
            "\nExamples:\n"
            + HelpExampleCli("listpreviousblockhashes", "")
            + HelpExampleRpc("listpreviousblockhashes", "")
            );

    int nHeight = chainActive.Height();
    int nStart = nHeight - 4;
    if (!(nHeight > 0) || !(nStart > 0))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Insufficient blocks connected to complete request!");

    std::vector<uint256> vHash;
    for (int i = nStart; i <= nHeight; i++) {
        uint256 hashBlock = chainActive[i]->GetBlockHash();
        vHash.push_back(hashBlock);
    }

    UniValue ret(UniValue::VARR);
    for (const uint256& hash : vHash) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("hash", hash.ToString()));
        ret.push_back(obj);
    }

    return ret;
}

UniValue listactivesidechains(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listactivesidechains\n"
            "List active sidechains.\n"
            "\nArguments:\n"
            "\nExamples:\n"
            + HelpExampleCli("listactivesidechains", "")
            + HelpExampleRpc("listactivesidechains", "")
            );

    std::vector<Sidechain> vActive = scdb.GetActiveSidechains();
    UniValue ret(UniValue::VARR);
    for (const Sidechain& s : vActive) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("title", s.title));
        obj.push_back(Pair("description", s.description));
        obj.push_back(Pair("privatekey", s.sidechainPriv));
        obj.push_back(Pair("keyid", s.sidechainKey));
        obj.push_back(Pair("hex", s.sidechainHex));

        ret.push_back(obj);
    }

    return ret;
}

UniValue listsidechainactivationstatus(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listsidechainactivationstatus\n"
            "List activation status of all pending sidechains.\n"
            "\nArguments:\n"
            "\nExamples:\n"
            + HelpExampleCli("listsidechainactivationstatus", "")
            + HelpExampleRpc("listsidechainactivationstatus", "")
            );

    std::vector<SidechainActivationStatus> vStatus;
    vStatus = scdb.GetSidechainActivationStatus();

    UniValue ret(UniValue::VARR);
    for (const SidechainActivationStatus& s : vStatus) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("title", s.proposal.title));
        obj.push_back(Pair("description", s.proposal.description));
        obj.push_back(Pair("privatekey", s.proposal.sidechainPriv));
        obj.push_back(Pair("keyid", s.proposal.sidechainKey));
        obj.push_back(Pair("hex", s.proposal.sidechainHex));
        obj.push_back(Pair("nage", s.nAge));
        obj.push_back(Pair("nfail", s.nFail));

        ret.push_back(obj);
    }

    return ret;
}

UniValue listsidechainproposals(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listsidechainproposals\n"
            "List your own cached sidechain proposals\n"
            "\nArguments:\n"
            "\nExamples:\n"
            + HelpExampleCli("listsidechainproposals", "")
            + HelpExampleRpc("listsidechainproposals", "")
            );

    std::vector<SidechainProposal> vProposal = scdb.GetSidechainProposals();
    UniValue ret(UniValue::VARR);
    for (const SidechainProposal& s : vProposal) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("title", s.title));
        obj.push_back(Pair("description", s.description));
        obj.push_back(Pair("privatekey", s.sidechainPriv));
        obj.push_back(Pair("keyid", s.sidechainKey));
        obj.push_back(Pair("hex", s.sidechainHex));

        ret.push_back(obj);
    }

    return ret;
}

UniValue getsidechainactivationstatus(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getsidechainactivationstatus\n"
            "List activation status for nSidechain.\n"
            "\nArguments:\n"
            "\nExamples:\n"
            + HelpExampleCli("getsidechainactivationstatus", "")
            + HelpExampleRpc("getsidechainactivationstatus", "")
            );

    // TODO
    std::vector<SidechainActivationStatus> vStatus;
    vStatus = scdb.GetSidechainActivationStatus();

    UniValue ret(UniValue::VARR);
    for (const SidechainActivationStatus& s : vStatus) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("title", s.proposal.title));
        obj.push_back(Pair("description", s.proposal.description));
        obj.push_back(Pair("privatekey", s.proposal.sidechainPriv));
        obj.push_back(Pair("keyid", s.proposal.sidechainKey));
        obj.push_back(Pair("scripthex", s.proposal.sidechainHex));
        obj.push_back(Pair("nage", s.nAge));
        obj.push_back(Pair("nfail", s.nFail));
        obj.push_back(Pair("proposalhash", s.proposal.GetHash().ToString()));

        ret.push_back(obj);
    }

    return ret;
}

UniValue createsidechainproposal(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "createsidechainproposal\n"
            "Generates a sidechain proposal to be included in the next block " \
            "mined by this node.\n"\
            "Note that this will not broadcast the proposal to other nodes. " \
            "You must mine a block which includes your proposal to complete " \
            "the process. Pending proposals created by this node will " \
            "automatically be included in the soonest block mined possible.\n"
            "\nArguments:\n"
            "1. \"title\"        (string, required) sidechain title\n"
            "2. \"description\"  (string, required) sidechain description\n"
            "3. \"privatekey\"   (string, required) sidechain private key\n"
            "\nExamples:\n"
            + HelpExampleCli("createsidechainproposal", "")
            + HelpExampleRpc("createsidechainproposal", "")
            );

    std::string strTitle = request.params[0].get_str();
    std::string strDescription = request.params[1].get_str();
    std::string strSecret = request.params[2].get_str();

    if (strTitle.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Sidechain must have a title!");

    // TODO maybe we should allow sidechains with no description? Anyways this
    // isn't a consensus rule right now
    if (strDescription.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Sidechain must have a description!");

    CBitcoinSecret vchSecret;
    if (!vchSecret.SetString(strSecret))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    CKey key = vchSecret.GetKey();
    if (!key.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();

    // Generate script hex
    CScript sidechainScript = CScript() << OP_DUP << OP_HASH160 << ToByteVector(vchAddress) << OP_EQUALVERIFY << OP_CHECKSIG;

    SidechainProposal proposal;
    proposal.title = strTitle;
    proposal.description = strDescription;
    proposal.sidechainPriv = strSecret;
    proposal.sidechainKey = HexStr(vchAddress);
    proposal.sidechainHex = HexStr(sidechainScript);

    // Cache proposal so that it can be added to the next block we mine
    scdb.CacheSidechainProposals(std::vector<SidechainProposal>{proposal});

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("title", proposal.title));
    obj.push_back(Pair("description", proposal.description));
    obj.push_back(Pair("privatekey", proposal.sidechainPriv));
    obj.push_back(Pair("keyid", proposal.sidechainKey));
    obj.push_back(Pair("hex", proposal.sidechainHex));

    return obj;
}

UniValue vote(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "vote\n"
            "Vote on sidechains and WT^(s) etc.\n"
            "\nArguments:\n"
            "1. \"type {}\"        (string, required) sidechain title\n"
            "\nExamples:\n"
            + HelpExampleCli("vote", "")
            + HelpExampleRpc("vote", "")
            );
    return NullUniValue;
}

UniValue echo(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            "echo|echojson \"message\" ...\n"
            "\nSimply echo back the input arguments. This command is for testing.\n"
            "\nThe difference between echo and echojson is that echojson has argument conversion enabled in the client-side table in"
            "drivenet-cli and the GUI. There is no server-side difference."
        );

    return request.params;
}

static UniValue getinfo_deprecated(const JSONRPCRequest& request)
{
    throw JSONRPCError(RPC_METHOD_NOT_FOUND,
        "getinfo\n"
        "\nThis call was removed in version 0.16.0. Use the appropriate fields from:\n"
        "- getblockchaininfo: blocks, difficulty, chain\n"
        "- getnetworkinfo: version, protocolversion, timeoffset, connections, proxy, relayfee, warnings\n"
        "- getwalletinfo: balance, keypoololdest, keypoolsize, paytxfee, unlocked_until, walletversion\n"
        "\ndrivenet-cli has the option -getinfo to collect and format these in the old format."
    );
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getmemoryinfo",          &getmemoryinfo,          {"mode"} },
    { "control",            "logging",                &logging,                {"include", "exclude"}},
    { "util",               "validateaddress",        &validateaddress,        {"address"} }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         {"nrequired","keys"} },
    { "util",               "verifymessage",          &verifymessage,          {"address","signature","message"} },
    { "util",               "signmessagewithprivkey", &signmessagewithprivkey, {"privkey","message"} },

    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            {"timestamp"}},
    { "hidden",             "echo",                   &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
    { "hidden",             "echojson",               &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
    { "hidden",             "getinfo",                &getinfo_deprecated,     {}},

    /* DriveChain rpc commands (mainly used by sidechains) */
    { "DriveChain",         "createcriticaldatatx",     &createcriticaldatatx,      {"amount", "height", "criticalhash"}},
    { "DriveChain",         "createbmmcriticaldatatx",  &createbmmcriticaldatatx,   {"amount", "height", "criticalhash", "nsidechain", "ndag"}},
    { "DriveChain",         "listsidechainctip",        &listsidechainctip,         {"nsidechain"}},
    { "DriveChain",         "listsidechaindeposits",    &listsidechaindeposits,     {"nsidechain", "count"}},
    { "DriveChain",         "countsidechaindeposits",   &countsidechaindeposits,    {"nsidechain"}},
    { "DriveChain",         "receivewtprime",           &receivewtprime,            {"nsidechain","rawtx"}},
    { "DriveChain",         "receivewtprimeupdate",     &receivewtprimeupdate,      {"height","update"}},
    { "DriveChain",         "getbmmproof",              &getbmmproof,               {"blockhash", "criticalhash"}},
    { "DriveChain",         "listpreviousblockhashes",  &listpreviousblockhashes,   {}},
    // Drivechain voting / sidechain activation rpc commands
    { "DriveChain",         "listactivesidechains",          &listactivesidechains,   {}},
    { "DriveChain",         "listsidechainactivationstatus", &listsidechainactivationstatus,   {}},
    { "DriveChain",         "listsidechainproposals",        &listsidechainproposals,   {}},
    { "DriveChain",         "getsidechainactivationstatus",  &getsidechainactivationstatus,   {}},
    { "DriveChain",         "createsidechainproposal",       &createsidechainproposal,   {"title", "description", "privatekey"}},
    { "DriveChain",         "vote",                          &vote,   {}},
};

void RegisterMiscRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
