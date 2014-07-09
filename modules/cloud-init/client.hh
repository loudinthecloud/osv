/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef CLIENT_HH_
#define CLIENT_HH_

//
// original code was taken from boost example:
// sync_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <streambuf>
#include <iterator>

namespace init {
/**
 * Sync client implementation based on the boost asio library
 * and the sync_client example.
 */

class client {
public:
    client();

    /**
     * open a connection to a path on a server
     * @param server a server to connect to
     * @param path the path on the server
     * @return a reference to the client
     */
    client& get(const std::string& server, const std::string& path, unsigned int port = 80);

    bool is_ok()
    {
        return status_code == 200;
    }

    unsigned int get_status()
    {
        return status_code;
    }

    operator bool () const {
        return !done;
    }

    friend std::ostream& operator<<(std::ostream& os, client& c);
private:
    void process_headers();
    boost::asio::streambuf response;
    unsigned int status_code;
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket _socket;
    bool done = false;
};

}
#endif /* CLIENT_HH_ */
