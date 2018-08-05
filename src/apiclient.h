// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef APICLIENT_H
#define APICLIENT_H

#include <amount.h>
#include <uint256.h>

#include <string>
#include <vector>

#include <boost/property_tree/json_parser.hpp>

class APIClient
{
public:
    APIClient();

    bool IsTxReplayed(const uint256& txid);

private:
    /*
     * Send json request
     */
    bool SendRequest(const std::string& json, boost::property_tree::ptree &ptree);
};

#endif // APICLIENT_H
