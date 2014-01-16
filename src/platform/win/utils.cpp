// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Linux-specific implementation of Utils.
 */

#include <vsm/exception.h>
#include <vsm/log.h>
#include <vsm/utils.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <algorithm>

using namespace vsm;
using namespace utils;

regex::regex_constants::syntax_option_type
vsm::platform_independent_filename_regex_matching_flag = regex::regex_constants::icase;

void
Make_nonblocking_handle(platform::Socket_handle handle)
{
    unsigned long arg = 1;
    if (ioctlsocket(handle, FIONBIO, &arg)) {
        VSM_EXCEPTION(Internal_error_exception, "Handle %d FIONBIO error [%d]: %s",
                handle, errno, strerror(errno));
    }
}
