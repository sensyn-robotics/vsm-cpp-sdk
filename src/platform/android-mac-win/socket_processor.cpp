// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Linux specific part of socket processor.
 */

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/log.h>

// CAN support is not available.
void
ugcs::vsm::Socket_processor::On_bind_can(
        Io_request::Ptr request,
        std::vector<int>,
        Stream::Ptr stream,
        std::string)
{
    request->Set_result_arg(Io_result::OTHER_FAILURE);
    stream->Set_connect_request(nullptr);
    request->Complete();
    return;
}
