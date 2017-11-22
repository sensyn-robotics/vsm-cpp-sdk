// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Io_stream class implementation
 */

#include <ugcs/vsm/io_stream.h>
#include <ugcs/vsm/debug.h>

#include <climits>

using namespace ugcs::vsm;

const Io_stream::Offset Io_stream::OFFSET_NONE = -1;
const Io_stream::Offset Io_stream::OFFSET_END = LLONG_MAX;

std::mutex Io_stream::name_mutex;

const char*
Io_stream::Io_result_as_char(const Io_result res)
{
    switch (res) {
    case Io_result::OK:
        return "OK";
    case Io_result::TIMED_OUT:
        return "TIMED_OUT";
    case Io_result::CANCELED:
        return "CANCELED";
    case Io_result::BAD_ADDRESS:
        return "BAD_ADDRESS";
    case Io_result::CONNECTION_REFUSED:
        return "CONNECTION_REFUSED";
    case Io_result::CLOSED:
        return "CLOSED";
    case Io_result::PERMISSION_DENIED:
        return "PERMISSION_DENIED";
    case Io_result::END_OF_FILE:
        return "END_OF_FILE";
    case Io_result::LOCK_ERROR:
        return "LOCK_ERROR";
    case Io_result::OTHER_FAILURE:
        return "OTHER_FAILURE";
    }
    VSM_EXCEPTION(Internal_error_exception,
            "Io_result value %d is not handled in stringify.",
            static_cast<int>(res));
    return "Invalid";
}

void
Io_stream::Add_ref()
{
    atomic_fetch_add(&ref_count, 1);
}

void
Io_stream::Release_ref()
{
    int res = atomic_fetch_sub(&ref_count, 1);
    if (res <= 0) {
        VSM_EXCEPTION(Internal_error_exception, "Reference counter underflow");
    } else if (res == 1 && state != State::CLOSED) {
        Close();
    }
}

std::string
Io_stream::Get_name() const
{
    std::unique_lock<std::mutex> lock(name_mutex);
    return name;
}

void
Io_stream::Set_name(const std::string& new_name)
{
    std::unique_lock<std::mutex> lock(name_mutex);
    name = new_name;
}
