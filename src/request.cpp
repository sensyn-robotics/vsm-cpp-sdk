// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Request class implementation.
 */

#include <ugcs/vsm/debug.h>
#include <ugcs/vsm/request_container.h>

using namespace ugcs::vsm;

Request::~Request()
{
    ASSERT(!processing_handler);
    ASSERT(!completion_context);
    ASSERT(!completion_handler);
    ASSERT(Is_done());
}

void
Request::Set_processing_handler(const Handler &handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    processing_handler = handler;
}

void
Request::Set_processing_handler(Handler &&handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    processing_handler = std::move(handler);
}

void
Request::Set_completion_handler(const Request_container::Ptr &context,
                                const Handler &handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    if (!!context != !!handler) {
        VSM_EXCEPTION(Invalid_op_exception,
                "Completion handler can not be set without completion context "
                "and vice versa.");
    }
    completion_context = context;
    completion_handler = handler;
}

void
Request::Set_completion_handler(const Request_container::Ptr &context,
                                Handler &&handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    if (!!context != !!handler) {
        VSM_EXCEPTION(Invalid_op_exception,
                "Completion handler can not be set without completion context "
                "and vice versa.");
    }
    completion_context = context;
    completion_handler = std::move(handler);
}

void
Request::Set_cancellation_handler(const Handler &handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    cancellation_handler = handler;
}

void
Request::Set_cancellation_handler(Handler &&handler)
{
    if (status != Status::PENDING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request not in pending state");
    }
    cancellation_handler = std::move(handler);
}

void
Request::Set_done_handler(Handler &handler)
{
    Locker lock(mutex);
    bool is_done = Is_done();
    if (!is_done) {
        done_handler = handler;
    }
    lock.unlock();
    if (is_done && handler) {
        handler();
    }
}

void
Request::Set_done_handler(Handler &&handler)
{
    Locker lock(mutex);
    bool is_done = Is_done();
    if (!is_done) {
        done_handler = handler;
    }
    lock.unlock();
    if (is_done && handler) {
        handler();
    }
}

Request_container::Ptr
Request::Get_completion_context(Locker locker) const
{
    Locker lock;
    if (locker) {
        ASSERT(locker.mutex() == &mutex);
        lock = std::move(locker);
    } else {
        lock = Lock();
    }
    return completion_context;
}

void
Request::Process(bool process_request)
{
    Locker lock(mutex);
    if (status == Status::ABORTED) {
        /* Do nothing if request was aborted. It is typical case when request is
         * aborted by some third party while it is still queued in some
         * container.
         */
        ASSERT(!completion_handler);
        return;
    }
    if (status == Status::ABORT_PENDING) {
        if (process_request) {
            /* Only request completion processing finalizes the pending state. */
            return;
        }
        /* Completion handler cleanup needs to be done as final step in
         * request deletion. */
        status = Status::ABORTED;
        ASSERT(completion_handler);
        ASSERT(!completion_context);
        Handler completion_handler_tmp = std::move(completion_handler);
        Destroy();
        /* Possible destruction of the user provided completion handler may have
         * arbitrary side effects, so do it outside the lock.
         */
        lock.unlock();
        return;
    }
    if (process_request) {
        if (status != Status::PENDING && status != Status::CANCELLATION_PENDING) {
            VSM_EXCEPTION(Invalid_op_exception,
                          "Attempted to process request in invalid state, state was %d!", status.load());
        }
        if (!processing_handler) {
            VSM_EXCEPTION(Nullptr_exception, "Processing handler not set");
        }
        if (status == Status::PENDING) {
            status = Status::PROCESSING;
        } else {
            status = Status::CANCELING;
        }
        cond_var.notify_all();
        /* Invoke processing handler. */
        Handler handler = std::move(processing_handler);
        lock.unlock();
        handler();
    } else {
        if (status < Status::RESULT_CODES) {
            VSM_EXCEPTION(Invalid_op_exception,
                          "Attempted to process request notification in invalid state");
        }
        Request_container::Ptr comp_ctx;
        Handler comp_handler;
        if (completion_handler) {
            /* Abort cannot be done after that. */
            comp_ctx = std::move(completion_context);
            comp_handler = std::move(completion_handler);
            lock.unlock();
            comp_handler();
            lock.lock();
            completion_delivered = true;
            /* Notify waiter to wake up operation waiters. */
            comp_ctx->Get_waiter()->Notify();
            cond_var.notify_all();
            Destroy();
        }
        Handler handler = std::move(done_handler);
        lock.unlock();
        if (handler) {
            handler();
        }
    }
}

void
Request::Complete(Status status, Locker locker)
{
    Locker lock;
    if (locker) {
        ASSERT(locker.mutex() == &mutex);
        lock = std::move(locker);
    } else {
        lock = Lock();
    }

    if (this->status == Status::ABORTED ||
        this->status == Status::ABORT_PENDING) {
        /* Do nothing if the request was aborted. It might be asynchronously
         * canceled by some third party.
         */
        return;
    }
    if (this->status != Status::PROCESSING &&
        this->status != Status::CANCELING) {
        VSM_EXCEPTION(Invalid_op_exception, "Request is not in valid state");
    }
    if (completion_processed) {
        VSM_EXCEPTION(Invalid_op_exception, "Request is already completed");
    }
    if (status < Status::RESULT_CODES) {
        VSM_EXCEPTION(Invalid_param_exception, "Disallowed status value specified");
    }
    this->status = status;
    Handler cancellation_handler_tmp = std::move(cancellation_handler);
    /* Submit to target completion context only if completion context specified. */
    completion_processed = true;
    cond_var.notify_all();
    if (completion_context) {
        Request_container::Ptr comp_ctx = completion_context;
        lock.unlock();
        comp_ctx->Submit_request(Shared_from_this());
    } else {
        /* No notification requested. Destroy this request. */
        completion_delivered = true;
        Handler handler = std::move(done_handler);
        Destroy();
        lock.unlock();
        if (handler) {
            handler();
        }
    }
}

void
Request::Cancel(Locker locker)
{
    Locker lock;
    if (locker) {
        ASSERT(locker.mutex() == &mutex);
        lock = std::move(locker);
    } else {
        lock = Lock();
    }
    if (status == Status::PENDING) {
        /* Not yet taken from input queue, just change status. */
        status = Status::CANCELLATION_PENDING;
    } else if (status == Status::PROCESSING) {
        /* Processing in progress, invoke cancellation handler if any. It is up
         * to processor to take any action if possible.
         */
        if (cancellation_handler) {
            Handler handler = std::move(cancellation_handler);
            lock.unlock();
            handler();
        }
    }
}

void
Request::Abort(Locker locker)
{
    Locker lock;
    if (locker) {
        ASSERT(locker.mutex() == &mutex);
        lock = std::move(locker);
    } else {
        lock = Lock();
    }

    if (Is_completion_delivering_started() || Is_done()) {
        return;
    }

    bool submit_needed = false;
    if (completion_handler) {
        /* Completion handler is present, release in completion
         * context should take place. Note that request could be already
         * completed (i.e. submitted to completion context).
         */
        status = Status::ABORT_PENDING;
        if (!completion_processed) {
            /* Request is not yet submitted to completion context. No one will
             * submit asynchronously, because of the status. So remember, that we
             * should do it. */
            submit_needed = true;
        }
    } else {
        status = Status::ABORTED;
    }

    if (cancellation_handler && status == Status::PROCESSING) {
        /* Invoke cancellation handler if the request is currently being processed. */
        Handler handler = std::move(cancellation_handler);
        lock.unlock();
        /* Status may switch from pending to aborted outside the lock if the
         * request was already submitted to completion context. */
        handler();
        handler = Handler();
        lock.lock();
    }
    /* Destroy possible cyclic references. */
    Handler proc_handler_tmp = std::move(processing_handler);
    Request_container::Ptr completion_context_tmp = std::move(completion_context);
    Handler cancellation_handler_tmp = std::move(cancellation_handler);
    if (status == Status::ABORTED && !submit_needed) {
        /* Fully completed request. */
        Destroy();
    }
    if (completion_context_tmp) {
        completion_context_tmp->Get_waiter()->Notify();
    }
    cond_var.notify_all();
    Handler handler = std::move(done_handler);
    lock.unlock();
    if (handler) {
        handler();
    }
    if (submit_needed) {
        ASSERT(status == Status::ABORT_PENDING);
        ASSERT(!completion_processed);
        completion_context_tmp->Submit_request(Shared_from_this());
    }
}

Request::Locker
Request::Lock(bool acquire) const
{
    return acquire ? Locker(mutex) : Locker(mutex, std::defer_lock_t());
}

void
Request::Destroy()
{
    /* Do nothing in base class. */
}

bool
Request::Is_completion_handler_present()
{
    return completion_handler ? true : false;
}

bool
Request::Wait_done(bool process_ctx, std::chrono::milliseconds timeout)
{
    Locker lock(mutex);
    if (Is_done()) {
        return true;
    }
    if (!process_ctx || !completion_context) {
        if (timeout.count()) {
            /* Prevent from request destruction. */
            Ptr request = Shared_from_this();
            cond_var.wait_for(lock, timeout, [&request](){ return request->Is_done(); });
            return Is_done();
        }
        while (!Is_done()) {
            cond_var.wait(lock);
        }
        return true;
    }
    Request_waiter::Ptr waiter = completion_context->Get_waiter();
    /* Prevent from request destruction. */
    Ptr request = Shared_from_this();
    auto predicate = [&request](){ return request->Is_done(); };
    // create a copy to avoid data race
    auto tmp_ctx = completion_context;
    lock.unlock();
    waiter->Wait_and_process({tmp_ctx}, timeout, 0,
                             Make_callback(predicate));
    return Is_done();
}
