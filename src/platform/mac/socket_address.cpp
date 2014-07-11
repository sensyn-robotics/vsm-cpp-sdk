// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * socket_address.cpp
 *
 *  Created on: May 10, 2013
 *      Author: janis
 *
 *  Mac specific part of sockets implementation
 */

// This file should be built only on Mac platforms
#include <ugcs/vsm/socket_address.h>

socklen_t
ugcs::vsm::Socket_address::Get_len()
{
    /**
     * Portability crap. MacOS send and bind is broken. It does not accept 128
     * as sddress length which is the length of struct sockaddr.
     * Use sizeof(struct sockaddr_in) for ipv4 instead.
     */

    if (is_resolved && Get_sockaddr_ref()->sa_family == AF_INET) {
        return sizeof(struct sockaddr_in);
    } else {
        return sizeof(storage);
    }
};

