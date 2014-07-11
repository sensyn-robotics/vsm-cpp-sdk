// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of @ref Piped_request_waiter
 */

#include <ugcs/vsm/piped_request_waiter.h>
#include <ugcs/vsm/log.h>

using namespace ugcs::vsm;

Piped_request_waiter::Piped_request_waiter()
{
    sockets::Init_sockets();
    if(sockets::Create_socketpair(read_pipe, write_pipe)) {
        VSM_EXCEPTION(Internal_error_exception, "Pipe creation error");
    }

    sockets::Make_nonblocking(read_pipe);
    sockets::Make_nonblocking(write_pipe);
}

Piped_request_waiter::~Piped_request_waiter()
{
    sockets::Close_socket(read_pipe);
    sockets::Close_socket(write_pipe);
    sockets::Done_sockets();
}

bool
Piped_request_waiter::Wait(std::chrono::milliseconds timeout)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(read_pipe, &rfds);

    timeval tv = {0, 0};
    timeval* tvp = &tv;
    if (timeout.count() > 0) {
        /* Calculate full seconds and remainder in microseconds. */
        std::chrono::seconds full_sec =
                std::chrono::duration_cast<std::chrono::seconds>(timeout);
        tv.tv_sec = full_sec.count();
        timeout -= full_sec;
        tv.tv_usec = std::chrono::microseconds(timeout).count();
    } else if (timeout.count() < 0) {
        tvp = nullptr;
    }

    int rc = select(read_pipe + 1, &rfds, nullptr, nullptr, tvp);

    if (rc == 0) {
        /* Timeout occurred. */
        return false;
    } else if (rc < 0) {
        VSM_SYS_EXCEPTION("Wait pipe wait error");
    }

    Ack();

    return true;
}

void
Piped_request_waiter::Ack()
{
    /* Consume notification event. */
    uint8_t event[1];

    int rc = recv(read_pipe, reinterpret_cast<char*>(event), 1, 0);
    notified = false;
    if (rc == SOCKET_ERROR) {
        if (!sockets::Is_last_operation_pending()) {
            VSM_SYS_EXCEPTION("Wait pipe read error");
        }
        /*
         * Someone has read all the data from the pipe from other thread, but
         * consider it OK for now, just say to the caller that pipe has signaled
         * successfully.
         * XXX Think more about the cases with multiple waiting threads.
         */
    } else if (rc == 0) {
        VSM_SYS_EXCEPTION("Wait pipe read zero");
    }
}

void
Piped_request_waiter::Notify()
{
    bool already_notified = notified.exchange(true);
    if (already_notified) {
        return;
    }
    ssize_t rc = send(write_pipe, "x", 1, sockets::SEND_FLAGS );
    if (rc == SOCKET_ERROR) {
        VSM_SYS_EXCEPTION("Notify pipe write error");
    } else if (rc == 0) {
        VSM_SYS_EXCEPTION("Notify pipe write returned zero");
    }
}
