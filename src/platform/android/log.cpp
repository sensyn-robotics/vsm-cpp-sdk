// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/log.h>
#include <android/log.h>

using namespace ugcs::vsm;

namespace {

class Android_logger: public Log::Platform_logger {
public:
    Android_logger(Log &log);

    virtual void
    Write_message(Log::Level level, const char *msg, std::va_list args) override
        __FORMAT(printf, 3, 0);

    static android_LogPriority
    Map_priority(Log::Level level);
};

} /* anonymous namespace */

Android_logger::Android_logger(Log &log):
    Log::Platform_logger(log)
{}

android_LogPriority
Android_logger::Map_priority(Log::Level level)
{
    switch (level) {
    case Log::Level::DEBUGGING:
        return ANDROID_LOG_DEBUG;
    case Log::Level::INFO:
        return ANDROID_LOG_INFO;
    case Log::Level::WARNING:
        return ANDROID_LOG_WARN;
    case Log::Level::ERROR:
        return ANDROID_LOG_ERROR;
    }
    return ANDROID_LOG_DEBUG;
}

void
Android_logger::Write_message(Log::Level level, const char *msg, std::va_list args)
{
    __android_log_vprint(Map_priority(level), "UGCS VSM", msg, args);
}

std::unique_ptr<Log::Platform_logger>
Log::Platform_logger::Create(Log &log)
{
    return std::unique_ptr<Log::Platform_logger>(new Android_logger(log));
}

int
Log::Vprintf_utf8(const char *format, std::va_list args)
{
    std::va_list printf_args;
    va_copy(printf_args, args);
    int ret = std::vprintf(format, printf_args);
    va_end(printf_args);
    return ret;
}
