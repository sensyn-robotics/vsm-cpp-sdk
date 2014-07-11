// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file request_temp_completion_context.h
 */

#ifndef TASK_TEMP_COMPLETION_CONTEXT_H_
#define TASK_TEMP_COMPLETION_CONTEXT_H_

#include <ugcs/vsm/request_context.h>

namespace ugcs {
namespace vsm {

/**  Temporal request completion context. */
class Request_temp_completion_context: public Request_completion_context {
    DEFINE_COMMON_CLASS(Request_temp_completion_context, Request_container)
public:

    Request_temp_completion_context() :
        Request_completion_context("Temporary completion context")
    {
        /* Always enable itself at creation. */
        Enable();
    }

    ~Request_temp_completion_context()
    {
        /* Always disable itself at destruction. */
        Disable();
    }

    /** Get this container type. */
    virtual Type
    Get_type() const override
    {
        return Type::TEMP_COMPLETION_CONTEXT;
    }

};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* TASK_TEMP_COMPLETION_CONTEXT_H_ */
