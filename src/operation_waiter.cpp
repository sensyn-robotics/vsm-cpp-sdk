// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Operation waiter implementation.
 */

#include <ugcs/vsm/operation_waiter.h>

using namespace ugcs::vsm;

Operation_waiter::~Operation_waiter()
{
    Destroy();
}

Operation_waiter&
Operation_waiter::operator=(Operation_waiter&& from)
{
    Destroy();
    request = std::move(from.request);
    return *this;
}

bool
Operation_waiter::Wait(bool process_ctx,
                       std::chrono::milliseconds timeout)
{
    if (!request) {
        return true;
    }
    return request->Wait_done(process_ctx, timeout);
}

void
Operation_waiter::Cancel()
{
    if (request) {
        request->Cancel();
    }
}

void
Operation_waiter::Abort()
{
    if (request) {
        request->Abort();
    }
}

void
Operation_waiter::Timeout(std::chrono::milliseconds timeout,
                               Timeout_handler handler,
                               bool cancel_operation,
                               Request_completion_context::Ptr ctx)
{
    Request_container::Ptr completion_ctx;
    if (!ctx) {
        completion_ctx = request->Get_completion_context();
    } else {
        completion_ctx = ctx;
    }
    /* If the completion context is not present, it is assumed that the
     * request has been already completed. The situation when it is wasn't
     * present at all is not supported. */
    if (completion_ctx) {
        auto timer = Timer_processor::Get_instance()->Create_timer(
                timeout,
                Make_callback(
                        &Operation_waiter::Timeout_cbk,
                        request,
                        handler,
                        cancel_operation),
                completion_ctx);
        request->Set_done_handler(Make_callback(&Operation_waiter::Request_done_cbk, timer));
    }
}

void
Operation_waiter::Destroy()
{
    if (!request) {
        return;
    }
    /* Always synchronize operations in temporal contexts. */
    Request_container::Ptr ctx = request->Get_completion_context();
    if (ctx && (ctx->Check_type(Request_container::Type::TEMPORAL))) {
        request->Wait_done(true);
    }
}

bool
Operation_waiter::Timeout_cbk(Request::Ptr request, Timeout_handler handler,
                              bool cancel_operation)
{
    request->Set_done_handler(nullptr);
    auto request_lock = request->Lock();
    request->Timed_out() = true;
    if (request->Is_completed() || request->Is_aborted()) {
        return false;
    }
    if (cancel_operation) {
        request->Cancel(std::move(request_lock));
    } else {
        request_lock.unlock();
    }
    if (handler) {
        handler(Ptr(new Operation_waiter(request)));
    }
    return false;
}

void
Operation_waiter::Request_done_cbk(Timer_processor::Timer::Ptr timer)
{
    timer->Cancel();
}
