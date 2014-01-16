// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * sockets.cpp
 *
 *  Created on: May 10, 2013
 *      Author: janis
 *
 *  Linux specific part of sockets implementation
 */

// This file should be built only on unix-based platforms
#ifdef __unix__

#include <vsm/log.h>
#include <vsm/exception.h>
#include <vsm/platform_sockets.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

void
platform::Init_sockets()
{
};

void
platform::Done_sockets()
{
};

int
platform::Create_socketpair(platform::Socket_handle& sock1, platform::Socket_handle& sock2)
{
    int fd[2];
    int ret = socketpair(AF_LOCAL, SOCK_RAW, 0, fd);
    if (ret == 0)
    {
        sock1 = fd[0];
        sock2 = fd[1];
    }
    return ret;
}

int
platform::Close_socket(platform::Socket_handle s)
{
    return close(s);
}

bool
platform::Is_last_operation_pending()
{
    return (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK);
}

int
platform::Make_nonblocking(platform::Socket_handle handle)
{
    int flags = fcntl(handle, F_GETFL);
    if (flags == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    return fcntl(handle, F_SETFL, flags | O_NONBLOCK);
}

int
platform::Prepare_for_listen(platform::Socket_handle handle)
{
    // Let it reuse addr. Do not do this on windows!
    int optval = 1;
    return setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

#endif // __unix__
