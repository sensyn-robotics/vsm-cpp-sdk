// Copyright (c) 2017, Smart Projects Holdings Ltd
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
#if defined(__unix__) || defined(__APPLE__)

#include <ugcs/vsm/log.h>
#include <ugcs/vsm/exception.h>
#include <ugcs/vsm/sockets.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

using namespace ugcs::vsm;

void
ugcs::vsm::sockets::Init_sockets()
{
};

void
ugcs::vsm::sockets::Done_sockets()
{
};

int
ugcs::vsm::sockets::Close_socket(sockets::Socket_handle s)
{
    return close(s);
}

bool
ugcs::vsm::sockets::Is_last_operation_pending()
{
    return (errno == EINPROGRESS) || (errno == EAGAIN) || (errno == EWOULDBLOCK);
}

int
ugcs::vsm::sockets::Make_nonblocking(sockets::Socket_handle handle)
{
    if (Disable_sigpipe(handle) != 0) {
        return -1;
    }

    int flags = fcntl(handle, F_GETFL);
    if (flags == INVALID_SOCKET) {
        return -1;
    }
    return fcntl(handle, F_SETFL, flags | O_NONBLOCK);
}

#endif // __unix__
