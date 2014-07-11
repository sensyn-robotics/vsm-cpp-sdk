// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file request_worker.h
 *
 * Request worker.
 */

#ifndef TASK_WORKER_H_
#define TASK_WORKER_H_

#include <ugcs/vsm/request_context.h>

#include <thread>

namespace ugcs {
namespace vsm {

/** Class for convenient worker thread instantiation for processing requests in
 * a set request containers.
 */
class Request_worker: public Request_completion_context {
    DEFINE_COMMON_CLASS(Request_worker, Request_container)
public:

    /** Construct worker. */
    Request_worker(const std::string& name) :
        Request_completion_context(name) {}

    /** Construct request worker by list of containers to work with. */
    Request_worker(
            const std::string& name,
            const std::initializer_list<Request_container::Ptr> &containers);

    /** Construct request worker by list of containers to work with. */
    Request_worker(
            const std::string& name,
            const std::list<Request_container::Ptr> &containers);

    /** Get this container type. */
    virtual Type
    Get_type() const override
    {
        return Type::ANY;
    }

    /** Enable all containers belonging to this worker. */
    void
    Enable_containers();

    /** Disable all containers belonging to this worker. */
    void
    Disable_containers();

private:
    /** Dedicated thread. */
    std::thread thread;
    /** Associated containers. */
    std::list<Request_container::Ptr> containers;

    /** Handle container enabling. */
    virtual void
    On_enable() override;

    /** Handle disabling request. */
    virtual void
    On_disable() override;

    /** Implement own wait-and-process transaction. */
    virtual void
    On_wait_and_process() override;

    /** Request processing or completion is called depending on the current
     * request state.
     */
    virtual void
    Process_request(Request::Ptr request) override;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* TASK_WORKER_H_ */
