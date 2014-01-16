// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *   Log class partial implementation (platform dependent).
 */

#include <vsm/log.h>

#include <cstring>
#include <sstream>

#include <string.h>
#include <syslog.h>

using namespace vsm;

namespace {

class Unix_logger: public Log::Platform_logger {
public:
    Unix_logger(Log &log);

    virtual void
    Write_message(Log::Level level, const char *msg, std::va_list args) override
        __FORMAT(printf, 3, 0);

    static int
    Map_priority(Log::Level level);
};

} /* anonymous namespace */

Unix_logger::Unix_logger(Log &log):
    Log::Platform_logger(log)
{
    openlog("VSM", LOG_PID, LOG_DAEMON);
}

int
Unix_logger::Map_priority(Log::Level level)
{
    switch (level) {
    case Log::Level::DEBUGGING:
        return LOG_DEBUG;
    case Log::Level::INFO:
        return LOG_INFO;
    case Log::Level::WARNING:
        return LOG_WARNING;
    case Log::Level::ERROR:
        return LOG_ERR;
    }
    return LOG_DEBUG;
}

void
Unix_logger::Write_message(Log::Level level, const char *msg, std::va_list args)
{
    vsyslog(Map_priority(level), msg, args);
}

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

std::unique_ptr<Log::Platform_logger>
Log::Platform_logger::Create(Log &log)
{
    return std::unique_ptr<Log::Platform_logger>(new Unix_logger(log));
}
