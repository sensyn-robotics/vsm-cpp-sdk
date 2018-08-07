// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file piped_request_waiter.h
 */
#ifndef _UGCS_VSM_PIPED_REQUEST_WAITER_H_
#define _UGCS_VSM_PIPED_REQUEST_WAITER_H_

#include <ugcs/vsm/request_container.h>
#include <ugcs/vsm/utils.h>
#include <ugcs/vsm/sockets.h>

#include <atomic>

namespace ugcs {
namespace vsm {

/** Request waiter which uses a pipe to signal about request submissions. */
class Piped_request_waiter : public Request_waiter {
    DEFINE_COMMON_CLASS(Piped_request_waiter, Request_waiter)

public:
    /** Constructor. */
    Piped_request_waiter();

    virtual
    ~Piped_request_waiter();

    virtual void
    Notify() override;

    /** Wait for an event for specific amount of time. */
    bool
    Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

    /** Consume one notification event. */
    void
    Ack();

    /** Get the platform handler of the wait pipe. */
    sockets::Socket_handle
    Get_wait_pipe()
    {
        return read_pipe;
    }

private:
    /** Pipe used to generate notifications. */
    sockets::Socket_handle write_pipe = INVALID_SOCKET;

    /** Pipe used to wait for notifications. */
    sockets::Socket_handle read_pipe = INVALID_SOCKET;

    /** TRUE if there are already some pending events which are not yet
     * processed, FALSE otherwise.
     */
    std::atomic_bool notified = { false };
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_PIPED_REQUEST_WAITER_H_ */
