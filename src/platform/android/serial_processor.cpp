// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Android-specific implementation.
 */

#include <ugcs/vsm/serial_processor.h>

using namespace ugcs::vsm;

/** Maximum possible value for termios VMIN parameter. */
const uint8_t ugcs::vsm::Serial_processor::MAX_VMIN = 255;

std::list<std::string>
Serial_processor::Enumerate_port_names()
{
    return std::list<std::string>();
}

Serial_processor::Stream::Native_handle::Unique_ptr
Serial_processor::Open_native_handle(const std::string &port_name, const Stream::Mode &mode)
{
    VSM_EXCEPTION(ugcs::vsm::Internal_error_exception, "not implemented");
}
