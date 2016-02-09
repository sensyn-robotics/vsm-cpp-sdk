// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Request container class implementation.
 */

#include <ugcs/vsm/debug.h>
#include <ugcs/vsm/request_container.h>

using namespace ugcs::vsm;

Request_container::Request_container(
        const std::string& name,
        Request_waiter::Ptr waiter):
    waiter(waiter), name(name)
{
}

Request_container::~Request_container()
{
    /* Should either be never enabled or properly disabled before destruction.
     * Disabling from here is not possible since derived classes destructors
     * already called and shared pointer to this instance also cannot be longer
     * retrieved (last reference is gone and base enable_shared_from_this cannot
     * longer return pointers).
     */
    ASSERT(!is_enabled);
}

void
Request_container::Submit_request(
        Request::Ptr request)
{
    Submit_request_impl(request, waiter->Lock_notify());
}

void
Request_container::Submit_request_locked(
        Request::Ptr request,
        Request_waiter::Locker locker)
{
    Submit_request_impl(request, std::move(locker));
}

int
Request_container::Process_requests(int requests_limit)
{
    Request::Ptr request;
    int num_processed = 0;
    while (!requests_limit || requests_limit > num_processed) {
        auto lock = waiter->Lock();
        if (request_queue.empty()) {
            break;
        }
        request = request_queue.front();
        request_queue.pop_front();
        lock.Unlock();
        Process_request(request);
        num_processed++;
    };
    return num_processed;
}

int
Request_container::Process_requests(std::unique_lock<std::mutex> &lock, int requests_limit)
{
    Request::Ptr request;
    int num_processed = 0;
    while (!requests_limit || requests_limit > num_processed) {
        if (request_queue.empty()) {
            break;
        }
        request = request_queue.front();
        request_queue.pop_front();
        lock.unlock();
        Process_request(request);
        lock.lock();
        num_processed++;
    };
    return num_processed;
}

void
Request_container::Set_waiter(Request_waiter::Ptr waiter)
{
    if (!waiter) {
        VSM_EXCEPTION(Nullptr_exception, "Null waiter provided");
    }
    //XXX check use cases and atomicity
    this->waiter = waiter;
}

void
Request_container::Enable()
{
    if (is_enabled.exchange(true)) {
        VSM_EXCEPTION(Invalid_op_exception, "Container already enabled: %s",
                      name.c_str());
    }
    On_enable();
}

void
Request_container::Disable()
{
    auto lock = waiter->Lock_notify();
    if (disable_ongoing.exchange(true)) {
        /* Already ongoing. Not a problem as such, but as per current design
         * this is not a usual situation, so throw a message at least. */
        LOG_INFO("Repeated disable of the request container: %s",
                 name.c_str());
        return;
    }
    lock.Unlock();
    On_disable();
    /* Derived classes should change the is_enabled when appropriate for them. */
    ASSERT(!is_enabled);
    /* Just in case, for release. */
    Set_disabled();

    /* Abort remaining requests. */
    Abort_requests();

    lock.Lock();
    if (!request_queue.empty()) {
        VSM_EXCEPTION(Internal_error_exception,
                "%zu requests still present after container is disabled.",
                request_queue.size());
    }
}

bool
Request_container::Is_enabled() const
{
    return is_enabled;
}

void
Request_container::On_enable()
{
    /* Do nothing in base class. */
}

void
Request_container::On_disable()
{
    /* There are no async operations in base class, so just disable. */
    Set_disabled();
}

void
Request_container::Set_disabled()
{
    auto locker = waiter->Lock_notify();
    is_enabled = false;
}

void
Request_container::Abort_requests()
{
    auto lock = waiter->Lock();
    abort_ongoing = true;
    bool cont = true;
    while (!request_queue.empty() && cont) {
        auto requests_copy = std::move(request_queue);
        lock.Unlock();

        for (auto& req: requests_copy) {
            req->Abort();
            /* Do process complete to finalize abort pending, if any. */
            req->Process(false);
        }
        requests_copy.clear();

        lock.Lock();
        cont = false;
        for(auto& req: request_queue) {
            if (req->Get_status() != Request::Status::ABORTED) {
                /* Full abort of one request generated another request.
                 * This is potentially error prone, so assert in debug,
                 * but try to recover in release.
                 */
                ASSERT(false);
                cont = true;
            }
        }
    }
    /* New submissions are not allowed after this at all. */
    abort_ongoing = false;
}

void
Request_container::On_wait_and_process()
{
    /* Trivial implementation for requests processing. */
    this->waiter->Wait_and_process({Shared_from_this()});
}

void
Request_container::Processing_loop()
{
    while (Is_enabled()) {
        On_wait_and_process();
    }
    auto lock = waiter->Lock();
    if (request_queue.size()) {
        LOG_DEBUG("Request container [%s] still has %zu requests after processing "
                  "loop exit.", name.c_str(), request_queue.size());
    }
}

void
Request_container::Submit_request_impl(
        Request::Ptr request,
        Request_waiter::Locker locker)
{
    ASSERT(locker.Want_notify());

    VERIFY(locker.Is_same_waiter(waiter), true);

    if (!Is_enabled()) {
        if (!abort_ongoing.load()) {
            VSM_EXCEPTION(Internal_error_exception,
                    "Request in state %d is submitted to fully disabled container [%s].",
                    static_cast<int>(request->Get_status()), name.c_str());
        }
        auto status = request->Get_status();
        if (status != Request::Status::ABORT_PENDING) {
            VSM_EXCEPTION(Internal_error_exception,
                    "Request in wrong state %d is submitted to disabled container [%s].",
                    static_cast<int>(status), name.c_str());
        }
    }
    request_queue.push_back(request);
}
