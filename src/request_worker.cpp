// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Request_worker class implementation.
 */

#include <vsm/request_worker.h>

using namespace vsm;

Request_worker::Request_worker(
        const std::string& name,
        const std::initializer_list<Request_container::Ptr> &containers) :
        Request_completion_context(name),
        containers(containers)
{
    /* Make sure containers handled by us indeed notify our waiter. */
    for (auto &container : containers) {
        container->Set_waiter(waiter);
    }
}

Request_worker::Request_worker(
        const std::string& name,
        const std::list<Request_container::Ptr> &containers) :
        Request_completion_context(name),
        containers(containers)
{
    /* Make sure containers handled by us indeed notify our waiter. */
    for (auto &container : containers) {
        container->Set_waiter(waiter);
    }
}

void
Request_worker::Enable_containers()
{
    for (auto &container : containers) {
        /* Skip this, because it is controlled by dedicated Enable method. */
        if (container != Shared_from_this()) {
            container->Enable();
        }
    }
}

void
Request_worker::Disable_containers()
{
    for (auto &container : containers) {
        /* Skip this, because it is controlled by dedicated Disable method. */
        if (container != Shared_from_this()) {
            container->Disable();
        }
    }
}

void
Request_worker::On_enable()
{
    Request_container::On_enable();
    containers.push_back(Shared_from_this());
    thread = std::thread(&Request_worker::Processing_loop, Shared_from_this());
}

void
Request_worker::On_disable()
{
    Set_disabled();
    thread.join();
    containers.remove(Shared_from_this());
    containers.clear();
}

void
Request_worker::On_wait_and_process()
{
    this->waiter->Wait_and_process(containers);
}

void
Request_worker::Process_request(Request::Ptr request)
{
    request->Process(request->Is_request_processing_needed());
}
