// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Request_waiter class implementation.
 */

#include <ugcs/vsm/defs.h>
#include <ugcs/vsm/request_container.h>
#include <ugcs/vsm/exception.h>

using namespace ugcs::vsm;

/* Request_waiter::Locker class implementation. */

Request_waiter::Locker::Locker(Request_waiter::Ptr waiter, bool want_notify):
    waiter(waiter), want_notify(want_notify), is_locked(true)
{
    waiter->mutex.lock();
}

Request_waiter::Locker::Locker(Locker&& other)
{
    this->waiter = std::move(other.waiter);
    this->want_notify = other.want_notify;
    this->is_locked = other.is_locked;
    /* Do not try to unlock anymore the waiter which is moved from. */
    other.is_locked = false;
}

Request_waiter::Locker::~Locker()
{
    if (is_locked) {
        Unlock();
    }
}

void
Request_waiter::Locker::Lock()
{
    ASSERT(waiter);
    if (is_locked) {
        VSM_EXCEPTION(Invalid_op_exception, "Already locked");
    }
    waiter->mutex.lock();
    is_locked = true;
}

void
Request_waiter::Locker::Unlock()
{
    ASSERT(waiter);
    if (!is_locked) {
        VSM_EXCEPTION(Invalid_op_exception, "Already unlocked");
    }
    waiter->mutex.unlock();
    is_locked = false;

    if (want_notify) {
        waiter->Notify();
    }
}

bool
Request_waiter::Locker::Want_notify() const
{
    return want_notify;
}

bool
Request_waiter::Locker::Is_same_waiter(const Request_waiter::Ptr& other) const
{
    return other == waiter;
}

/* Request_waiter class implementation. */

template <class Container_list>
int
Request_waiter::Wait_and_process_impl(const Container_list &containers,
                                   std::chrono::milliseconds timeout,
                                   int requests_limit, Predicate ext_predicate)
{
    std::unique_lock<std::mutex> lock(mutex);
    int total_processed = 0;

    /* Predicate for checking and processing requests if any. */
    auto predicate = [&]()
    {
        int num_processed = 0;
        bool is_disabled = false;
        int cur_processed;
        /* Do in rounds for a case when processing generates some requests for the
         * same containers.
         */
        do {
            cur_processed = 0;
            for (auto &container : containers) {
                if (!container->Is_enabled()) {
                    is_disabled = true;
                } else {
                    cur_processed += container->Process_requests(lock, requests_limit);
                    if (requests_limit && num_processed + cur_processed >= requests_limit) {
                        break;
                    }
                }
            }
            num_processed += cur_processed;
        } while (cur_processed && (!requests_limit || num_processed < requests_limit));
        total_processed += num_processed;
        return ext_predicate ? ext_predicate() : (num_processed || is_disabled);
    };

    if (predicate()) {
        return total_processed;
    }
    /* No requests in the queues, so the lock was not released, safe to wait. */
    if (timeout.count()) {
        cond_var.wait_for(lock, timeout, predicate);
    } else {
        cond_var.wait(lock, predicate);
    }
    return total_processed;
}

int
Request_waiter::Wait_and_process(const std::initializer_list<Request_container::Ptr> &containers,
                              std::chrono::milliseconds timeout,
                              int requests_limit, Predicate predicate)
{
    return Wait_and_process_impl(containers, timeout, requests_limit, predicate);
}

int
Request_waiter::Wait_and_process(const std::list<Request_container::Ptr> &containers,
                              std::chrono::milliseconds timeout,
                              int requests_limit, Predicate predicate)
{
    return Wait_and_process_impl(containers, timeout, requests_limit, predicate);
}

void
Request_waiter::Notify()
{
    std::unique_lock<std::mutex> lock(mutex);
    cond_var.notify_all();
}

Request_waiter::Locker
Request_waiter::Lock()
{
    return Locker(Shared_from_this(), false);
}

Request_waiter::Locker
Request_waiter::Lock_notify()
{
    return Locker(Shared_from_this(), true);
}
