// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * sockets.cpp
 *
 *  Created on: May 10, 2013
 *      Author: janis
 *
 *  Mac specific part of sockets implementation
 */

// This file should be built only on Linux platforms
#include <ugcs/vsm/sockets.h>

int
ugcs::vsm::sockets::Disable_sigpipe(sockets::Socket_handle)
{
    return 0;
}

int
ugcs::vsm::sockets::Create_socketpair(sockets::Socket_handle& sock1, sockets::Socket_handle& sock2)
{
    int fd[2];
    int ret = socketpair(AF_LOCAL,
#   ifdef ANDROID
            /* Android does not permit raw sockets creation. */
            SOCK_STREAM,
#   else
            SOCK_RAW,
#   endif
            0, fd);
    if (ret == 0) {
        sock1 = fd[0];
        sock2 = fd[1];
    }
    return ret;
}

int
ugcs::vsm::sockets::Prepare_for_listen(sockets::Socket_handle handle, bool)
{
    int optval = 1;
    return setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
