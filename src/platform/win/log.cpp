// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform dependent).
 */

#include <vsm/log.h>
#include <vsm/exception.h>

#include <cstdio>
#include <sstream>
#include <fstream>
#include <ctime>
#include <chrono>
#include <locale>

#include <windows.h>
#include <Userenv.h>

using namespace vsm;

namespace {

class Windows_logger: public Log::Platform_logger {
public:
    Windows_logger(Log &log);

    virtual void
    Write_message(Log::Level, const char *, std::va_list) override
    __FORMAT(printf, 3, 0) {};
};

} /* anonymous namespace */

Windows_logger::Windows_logger(Log &log):
    Log::Platform_logger(log)
{
}

std::string
Log::Get_system_error()
{
    DWORD code = GetLastError();
    char desc[1024];
    DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                desc, sizeof(desc), nullptr);
    /* Trim trailing new line. */
    if (size >= 2 && desc[size - 2] == 0xd && desc[size - 1] == 0xa) {
        size -= 2;
    }
    std::stringstream ss;
    ss << code << " - " << std::string(desc, size);
    return ss.str();
}

std::unique_ptr<Log::Platform_logger>
Log::Platform_logger::Create(Log &log)
{
    return std::unique_ptr<Log::Platform_logger>(new Windows_logger(log));
}
