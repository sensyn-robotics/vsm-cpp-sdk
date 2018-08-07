// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform dependent).
 */

#include <ugcs/vsm/log.h>
#include <string.h>
#include <sstream>

using namespace ugcs::vsm;

/* Properly handle both XSI-compliant and GNU-specific version of strerror_r. */
template <typename Ret_type>
struct Strerror {};

template <>
struct Strerror<int> {
    static const char *
    Call(int code, char *buf, size_t buf_size)
    {
        if (strerror_r(code, buf, buf_size) == 0) {
            return buf;
        }
        return nullptr;
    }
};

template <>
struct Strerror<char *> {
    static const char *
    Call(int code, char *buf, size_t buf_size)
    {
        /* Reinterpret cast to suppress warning when with XSI version. Actually
         * it is never applied because other specialization is selected for
         * XSI version.
         */
        return reinterpret_cast<char *>(strerror_r(code, buf, buf_size));
    }
};

std::string
Log::Get_system_error()
{
    int code = errno;
    char buf[1024];
    using Ret_type = decltype(strerror_r(code, buf, sizeof(buf)));
    const char *desc = Strerror<Ret_type>::Call(code, buf, sizeof(buf));
    std::stringstream ss;
    ss << code;
    if (desc) {
        ss << " - " << desc;
    }
    return ss.str();
}
