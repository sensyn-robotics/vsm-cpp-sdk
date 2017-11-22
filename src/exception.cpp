// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Exception class implementation.
 */

#include <ugcs/vsm/exception.h>
#include <ugcs/vsm/log.h>

#include <memory>
#include <vector>

using namespace ugcs::vsm;

Exception::Exception(Va_args_overload, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    Create_msg(msg, args);
    va_end(args);
}

Exception::Exception(Va_list_overload, const char *msg, va_list args)
{
    Create_msg(msg, args);
}

void
Exception::Create_msg(const char *msg, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, msg, args_copy);
    va_end(args_copy);
    auto buf = std::unique_ptr<std::vector<char>>(new std::vector<char>(size + 1));
    vsnprintf(&buf->front(), size + 1, msg, args);
    this->msg = &buf->front();
    LOG_DEBUG("Exception created: %s", this->msg.c_str());
}
