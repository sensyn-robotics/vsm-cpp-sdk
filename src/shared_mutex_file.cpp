// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_mutex_file.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: janis
 */

#include <ugcs/vsm/shared_mutex_file.h>

namespace ugcs {
namespace vsm {

Shared_mutex_file::~Shared_mutex_file()
{
    stream->Close();
}

Operation_waiter
Shared_mutex_file::Acquire(Shared_mutex_file::Acquire_handler completion_handler,
        Request_completion_context::Ptr comp_ctx)
{
    return stream->Lock(completion_handler, comp_ctx);
}

Operation_waiter
Shared_mutex_file::Release(Shared_mutex_file::Acquire_handler completion_handler,
        Request_completion_context::Ptr comp_ctx)
{
    return stream->Unlock(completion_handler, comp_ctx);
}


} /* namespace vsm */
} /* namespace ugcs */
