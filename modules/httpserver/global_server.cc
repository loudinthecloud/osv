/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "global_server.hh"
#include "api/os.hh"
#include "api/fs.hh"
#include "api/files_mapping.hh"
#include "api/jvm.hh"
#include "api/file.hh"
#include "api/trace.hh"
#include "api/env.hh"
#include "api/hardware.hh"
#include "path_holder.hh"

#include <iostream>

namespace httpserver {

global_server* global_server::instance = nullptr;

global_server& global_server::get()
{
    if (instance == nullptr) {
        instance = new global_server();
    }
    return *instance;
}

bool global_server::run()
{
    if (get().s != nullptr) {
        return false;
    }
    get().set("ipaddress", "0.0.0.0");
    get().set("port", "8000");
    get().s = new http::server::server(&get().config, &get()._routes);
    get().s->run(); // never returns from here
    return true;
}


global_server::global_server()
    : s(nullptr)
{
    set_routes();

}

global_server& global_server::set(po::variables_map& _config)
{
    for (auto i : _config) {
        get().config.insert(std::make_pair(i.first, i.second));
    }
    return *instance;
}

global_server& global_server::set(const std::string& key,
                                  const std::string& _value)
{
    boost::any val(_value);
    boost::program_options::variable_value v(val, false);
    config.insert(std::make_pair(std::string(key), v));
    return *this;

}

void global_server::set_routes()
{
    path_holder::set_routes(&_routes);
    api::os::init(_routes);
    api::fs::init(_routes);
    api::file::init(_routes);
    api::jvm::init(_routes);
    api::trace::init(_routes);
    api::env::init(_routes);
    api::files_mapping::init(_routes);
    api::hardware::init(_routes);
}

}
