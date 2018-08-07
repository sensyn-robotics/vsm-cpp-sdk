// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor implementation.
 */

#include <ugcs/vsm/serial_processor.h>

using namespace ugcs::vsm;

Singleton<Serial_processor> Serial_processor::singleton;

Serial_processor::Serial_processor()
{}

Serial_processor::Stream::Stream(Serial_processor::Ptr processor,
        const std::string& port_name,
        const Stream::Mode &mode,
        Native_handle::Unique_ptr&& native_handle):
    File_processor::Stream(processor, port_name,
            File_processor::Stream::Mode("r+"), false, std::move(native_handle)),
    processor(processor), mode(mode)
{
    stream_type = Io_stream::Type::SERIAL;
}

Serial_processor::Stream::Ref
Serial_processor::Open(const std::string &port_name, const Stream::Mode &mode)
{
    auto stream = Stream::Create(Shared_from_this(), port_name, mode,
            Open_native_handle(port_name, mode));
    if (stream)
        Register_stream(stream);
    return stream;
}
