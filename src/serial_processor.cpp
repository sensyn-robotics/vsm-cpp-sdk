// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor implementation.
 */

#include <vsm/serial_processor.h>

using namespace vsm;

/* ****************************************************************************/
/* Serial_processor::Stream class. */

Serial_processor::Stream::Stream(Serial_processor::Ptr processor,
                                 const std::string &port_name, const Mode &mode):
    File_processor::Stream(Open_handle(port_name, mode).get(), nullptr,
                           processor, port_name,
                           File_processor::Stream::Mode("r+"), false),
    processor(processor), mode(mode)
{

}

/* ****************************************************************************/
/* Serial_processor class. */

Singleton<Serial_processor> Serial_processor::singleton;

Serial_processor::Serial_processor()
{}

Serial_processor::Stream::Ref
Serial_processor::Open(const std::string &port_name, const Stream::Mode &mode)
{
    auto stream = Stream::Create(Shared_from_this(), port_name, mode);
    if (stream)
        Register_stream(stream);
    return stream;
}
