// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <apiclient.h>

#include <util.h>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/foreach.hpp>

using boost::asio::ip::tcp;

APIClient::APIClient()
{

}

bool APIClient::IsTxReplayed(const uint256& txid)
{
    // TODO use sendrequest instead of everything here
    // TODO everything (loops, reads) need to be safety checked
    // TODO needs to return false if request failed
    // TODO return replay status by refernce

    try {
        // Setup BOOST ASIO for a synchronus call to the mainchain
        boost::asio::io_service io_service;
        boost::asio::ssl::context sslContext(io_service, boost::asio::ssl::context::method::sslv23_client);
        boost::asio::ssl::stream<tcp::socket> sslSocket(io_service, sslContext);
        tcp::resolver resolver(io_service);

        tcp::resolver::query query("blockchain.info", "443");
        auto endpoint_iterator = resolver.resolve(query);

        boost::asio::connect(sslSocket.lowest_layer(), endpoint_iterator);

        sslSocket.handshake(boost::asio::ssl::stream_base::handshake_type::client);

        std::string strRequest = "/rawtx/";
        strRequest += txid.ToString();

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "GET " << strRequest << " HTTP/1.1\n";
        os << "Host: blockchain.info:443\n";
        os << "Accept: application/json\n";
        os << "Connection: close\n\n";

        // Send the request
        boost::asio::write(sslSocket, output);

        // Read the reponse
        std::string data;
        for (;;)
        {
            boost::array<char, 4096> buf;

            // Read until end of file (socket closed)
            boost::system::error_code e;
            size_t sz = sslSocket.read_some(boost::asio::buffer(buf), e);

            if (!sz)
                break;

            data.insert(data.size(), buf.data(), sz);

            if (e == boost::asio::error::eof)
                break; // socket closed
            else if (e)
                throw boost::system::system_error(e);
        }

        std::stringstream ss;
        ss << data;

        // Get response code
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        int code;
        ss >> code;

        // Check response code
        if (code != 200)
            return false;

        // TODO just use find / rfind and get substring of {} ?

        // Skip the rest of the header
        for (size_t i = 0; i < 24; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\r');

        for (size_t i = 0; i < 2; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Parse json response;
        std::stringstream JSON;
        JSON << ss.rdbuf();

        data.clear();
        data = JSON.str();

        // Create final JSON string with junk removed from the end
        auto rpos = data.rfind("}");
        std::string strJSON = data.substr(0, rpos + 3);

        std::stringstream jss;
        jss << strJSON;
        boost::property_tree::ptree ptree;
        boost::property_tree::json_parser::read_json(jss, ptree);

        // Did we find this transaction on the Bitcoin "Core" fork?
        BOOST_FOREACH(boost::property_tree::ptree::value_type &value, ptree.get_child("")) {
            if (value.first == "hash") {
                std::string hash = value.second.data();
                if (hash.empty())
                    continue;

                // If we find the Tx on Bitcoin "Core", it is replayed here
                if (txid == uint256S(hash)) {
                    return true;
                }
            }
        }
    } catch (std::exception &exception) {
        LogPrintf("ERROR API client (sendRequestToMainchain): %s\n", exception.what());
        return false;
    }

    return false;
}

bool APIClient::SendRequest(const std::string& json, boost::property_tree::ptree &ptree)
{
    // Mainnet RPC = 8332
    // Testnet RPC = 18332
    // Regtest RPC = 18443

    try {
        // Setup BOOST ASIO for a synchronus call to the mainchain
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        tcp::resolver::query query("blockchain.info", "443");
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::resolver::iterator end;

        tcp::socket socket(io_service);
        boost::system::error_code error = boost::asio::error::host_not_found;

        // Try to connect
        while (error && endpoint_iterator != end)
        {
          socket.close();
          socket.connect(*endpoint_iterator++, error);
        }

        if (error) throw boost::system::system_error(error);

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "POST / HTTP/1.1\n";
        os << "Host: 127.0.0.1\n";
        os << "Content-Type: application/json\n";
        //os << "Authorization: Basic " << EncodeBase64(auth) << std::endl;
        os << "Connection: close\n";
        os << "Content-Length: " << json.size() << "\n\n";
        os << json;

        // Send the request
        boost::asio::write(socket, output);

        // Read the reponse
        std::string data;
        for (;;)
        {
            boost::array<char, 4096> buf;

            // Read until end of file (socket closed)
            boost::system::error_code e;
            size_t sz = socket.read_some(boost::asio::buffer(buf), e);

            data.insert(data.size(), buf.data(), sz);

            if (e == boost::asio::error::eof)
                break; // socket closed
            else if (e)
                throw boost::system::system_error(e);
        }

        std::stringstream ss;
        ss << data;

        // Get response code
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        int code;
        ss >> code;

        // Check response code
        if (code != 200)
            return false;

        // Skip the rest of the header
        for (size_t i = 0; i < 5; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\r');

        // Parse json response;
        std::string JSON;
        ss >> JSON;
        std::stringstream jss;
        jss << JSON;
        boost::property_tree::json_parser::read_json(jss, ptree);
    } catch (std::exception &exception) {
        LogPrintf("ERROR API client (sendRequestToMainchain): %s\n", exception.what());
        return false;
    }
    return true;
}
