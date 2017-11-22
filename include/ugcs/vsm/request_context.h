// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file request_context.h
 *
 * Request execution context.
 */

#ifndef TASK_CONTEXT_H_
#define TASK_CONTEXT_H_

#include <ugcs/vsm/request_container.h>

namespace ugcs {
namespace vsm {

/** Request execution context.
 * @param is_processor Request processor if "true", request completion context
 *      otherwise.
 */
template <bool is_processor>
class Request_context: public Request_container {
    DEFINE_COMMON_CLASS(Request_context, Request_container)
public:
    /** Constructor. */
    using Request_container::Request_container;

    /** Get this container type. */
    virtual Type
    Get_type() const override
    {
        return is_processor ? Type::PROCESSOR : Type::COMPLETION_CONTEXT;
    }

private:
    /** Process request.
     * @param request Request to process.
     * @return Request processing status.
     */
    virtual void
    Process_request(Request::Ptr request) override
    {
        request->Process(is_processor);
    }
};

/** Request processor is a request execution context where request are processed. */
typedef Request_context<true> Request_processor;

/** Request completion context is a request execution context where notification about
 * request completions are processed.
 */
typedef Request_context<false> Request_completion_context;

} /* namespace vsm */
} /* namespace ugcs */

#endif /* TASK_CONTEXT_H_ */
