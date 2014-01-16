// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Linux-specific implementation of Utils.
 */

#include <vsm/utils.h>
#include <vsm/exception.h>
#include <vsm/log.h>

#include <fcntl.h>
#include <string.h>
#include <signal.h>

using namespace vsm;
using namespace utils;

regex::regex_constants::syntax_option_type
vsm::platform_independent_filename_regex_matching_flag = regex::regex_constants::none;

void
Make_nonblocking_handle(platform::Socket_handle handle)
{
    int flags = fcntl(handle, F_GETFL);
    if (flags == INVALID_SOCKET) {
        VSM_EXCEPTION(Internal_error_exception, "Handle %d F_GETFL error [%d]: %s",
                handle, errno, strerror(errno));
    }
    if (fcntl(handle, F_SETFL, flags | O_NONBLOCK)) {
        VSM_EXCEPTION(Internal_error_exception, "Handle %d F_SETFL(O_NONBLOCK) error [%d]: %s",
                handle, errno, strerror(errno));
    }
}

void
Ignore_signal(int signal_number)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    sa.sa_handler = SIG_IGN;
#pragma GCC diagnostic pop
    if (sigaction(signal_number, &sa, NULL)) {
        LOG_ERR("sigaction error: %s", strerror(errno));
    }
}
