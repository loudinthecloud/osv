/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef NETWORK_HH_
#define NETWORK_HH_

#include "routes.hh"

namespace httpserver {

namespace api {

namespace network {

/**
 * Initialize the routes object with specific routes mapping
 * @param routes - the routes object to fill
 */
void init(routes& routes);

}
}
}





#endif /* NETWORK_HH_ */
