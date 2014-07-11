// Copyright (c) 2014, Smart Projects Holdings Ltd
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
    const char *desc = strerror_r(code, buf, sizeof(buf));

    std::stringstream ss;
    ss << code << " - " << desc;
    return ss.str();
}
