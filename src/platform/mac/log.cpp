// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform dependent).
 */

#include <ugcs/vsm/log.h>

#include <sstream>
#include <string.h>

using namespace ugcs::vsm;

std::string
Log::Get_system_error()
{
    int code = errno;
    char buf[1024];
    std::stringstream ss;
    int ret = strerror_r(code, buf, sizeof(buf));
    if (ret == 0) {
        ss << code << " - " << buf;
    } else {
        ss << code << " - strerror_r failed: " << ret;
    }
    return ss.str();
}
