// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "dstencode.h"
#include "test/test_drivenet.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(dstencode_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_addresses) {
    std::vector<uint8_t> hash = {118, 160, 64,  83,  189, 160, 168,
                                 139, 218, 81,  119, 184, 106, 21,
                                 195, 178, 159, 85,  152, 115};

    const CTxBCHDestination dstKey = CKeyID(uint160(hash));
    const CTxBCHDestination dstScript = CScriptID(uint160(hash));

    std::string cashaddr_pubkey =
        "bitcoincash:qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a";
    std::string cashaddr_script =
        "bitcoincash:ppm2qsznhks23z7629mms6s4cwef74vcwvn0h829pq";
    std::string base58_pubkey = "1BpEi6DfDAUFd7GtittLSdBeYJvcoaVggu";
    std::string base58_script = "3CWFddi6m4ndiGyKqzYvsFYagqDLPVMTzC";

    // Check encoding
    BOOST_CHECK_EQUAL(cashaddr_pubkey, EncodeBCHDestination(dstKey));
    BOOST_CHECK_EQUAL(cashaddr_script, EncodeBCHDestination(dstScript));
    BOOST_CHECK_EQUAL(base58_script, EncodeBCHDestination(dstScript, true)); // Note: uses legacy encoding

    // Check decoding
    const CChainParams &params = Params();
    BOOST_CHECK(dstKey == DecodeBCHDestination(cashaddr_pubkey, params));
    BOOST_CHECK(dstScript == DecodeBCHDestination(cashaddr_script, params));
    BOOST_CHECK(dstKey == DecodeBCHDestination(base58_pubkey, params));
    BOOST_CHECK(dstScript == DecodeBCHDestination(base58_script, params));

    // Validation
    BOOST_CHECK(IsValidBCHDestinationString(cashaddr_pubkey, params));
    BOOST_CHECK(IsValidBCHDestinationString(cashaddr_script, params));
    BOOST_CHECK(IsValidBCHDestinationString(base58_pubkey, params));
    BOOST_CHECK(IsValidBCHDestinationString(base58_script, params));
    BOOST_CHECK(!IsValidBCHDestinationString("notvalid", params));
}

BOOST_AUTO_TEST_SUITE_END()
