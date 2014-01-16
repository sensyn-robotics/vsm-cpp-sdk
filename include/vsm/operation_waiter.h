// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file operation_waiter.h
 *
 * Operation waiter class.
 */

#ifndef OPERATION_WAITER_H_
#define OPERATION_WAITER_H_

#include <vsm/request_container.h>
#include <vsm/timer_processor.h>

namespace vsm {

/** Class for synchronizing with request execution. Instance of this class should
 * be returned by value from asynchronous methods of request processors. Only one
 * instance is allowed (copying is disabled).
 */
class Operation_waiter {
public:
    /** Pointer type when used as argument in callbacks. */
    typedef std::unique_ptr<Operation_waiter> Ptr;

    /** Timeout handler prototype. */
    typedef Callback_proxy<void, Ptr> Timeout_handler;

    /** Construct operation waiter.
     *
     * @param request Associated request. No request is associated if nullptr. In such
     *      case it is dummy waiter object which behaves like its associated
     *      request is already done.
     */
    Operation_waiter(Request::Ptr request = nullptr):
        request(request)
    {}

    /** Construct operation waiter from any type of request pointer. */
    template <class Request_type>
    Operation_waiter(std::shared_ptr<Request_type> request):
        Operation_waiter(Request::Ptr(request))
    {}

    /** Destructor which might wait for the request to be done. */
    ~Operation_waiter();

    /** Default move constructor. */
    Operation_waiter(Operation_waiter &&) = default;

    /** Copy constructor disabled. Having more than one copy of operation
     * waiter is confusing and error prone.
     */
    Operation_waiter(const Operation_waiter&) = delete;

    /** Move assignment operator. If the waiter instance which is assigned to
     * is not empty, it is properly destroyed. */
    Operation_waiter&
    operator=(Operation_waiter&&);

    /** Assignment operator is disabled. Having more than one copy of operation
     * waiter is confusing and error prone.
     */
    Operation_waiter&
    operator=(const Operation_waiter&) = delete;

    /** Wait for request is fully processed, i.e. all handlers were invoked and
     * no more actions pending.
     *
     * @param process_ctx Process all requests in the associated completion
     *      context. This may be useful if the context is normally
     *      processed in the calling thread.
     * @param timeout Timeout value for waiting. Zero value indicates
     *      indefinite waiting.
     * @return "true" if request was processed, "false" if the function
     *      returned by timeout expiration.
     */
    bool
    Wait(bool process_ctx = true,
         std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /** Schedule timeout for the operation. After the specified time the optional
     * callback is invoked and the operation is optionally canceled.
     *
     * @param timeout Timeout for the operation.
     * @param timeout_handler Handler which should be invoked when timeout
     *      elapses. The handler should have constant reference to
     *      Operation_waiter::Ptr as the first argument.
     * @param cancel_operation Cancel the operation after the timeout elapses if
     *      "true".
     * @param ctx Context used for handler invocation. By default the handler is
     *      invoked in request completion context, which should be present in
     *      this case.
     */
    void
    Timeout(std::chrono::milliseconds timeout,
            Timeout_handler timeout_handler = Timeout_handler(),
            bool cancel_operation = true,
            Request_completion_context::Ptr ctx = nullptr);

    /** Cancel the operation. This action behavior is defined by specific
     * operation and processor.
     */
    void
    Cancel();

    /** Abort the operation. Completion handler will not be executed, unless
     * it has been already executed or execution already started.
     */
    void
    Abort();

    /** Check if request is fully processed, i.e. all handlers were invoked and
     * no more actions pending.
     */
    bool
    Is_done() const
    {
        return request ? request->Is_done() : true;
    }

private:
    /** Associated request. */
    Request::Ptr request;

    /** Destroy the instance waiting for the associated request to be done
     * if the completion context is temporary.
     */
    void
    Destroy();

    /** Handler for timeout timer. Cannot use member function because object
     * life time can be short and is not controllable.
     *
     * @param handler User provided timeout handler.
     * @return
     */
    static bool
    Timeout_cbk(Request::Ptr request, Timeout_handler handler, bool cancel_operation);

    /** Handler for request finishing. */
    static void
    Request_done_cbk(Timer_processor::Timer::Ptr timer);
};

/** Convenience builder for timeout callbacks.
 * The callback should have pointer to Operation_waiter object as the first
 * argument.
 */
DEFINE_CALLBACK_BUILDER(Make_timeout_callback, (Operation_waiter::Ptr), (nullptr))

} /* namespace vsm */

#endif /* OPERATION_WAITER_H_ */
