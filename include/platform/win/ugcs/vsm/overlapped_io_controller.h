// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file overlapped_io_controller.h
 */

#ifndef _UGCS_VSM_OVERLAPPED_IO_CONTROLLER_H_
#define _UGCS_VSM_OVERLAPPED_IO_CONTROLLER_H_

#include <ugcs/vsm/windows_file_handle.h>

namespace ugcs {
namespace vsm {
namespace internal {

/** Windows-specific implementation for I/O controller. */
class Overlapped_io_controller: public File_processor::Native_controller {
public:
    Overlapped_io_controller();

    virtual
    ~Overlapped_io_controller();

    /** Enable the controller. */
    virtual void
    Enable() override;

    /** Disable the controller. */
    virtual void
    Disable() override;

    /** Register new opened file handle. */
    virtual void
    Register_handle(File_processor::Stream::Native_handle &handle) override;

    /** Unregister previously registered file handle. */
    virtual void
    Unregister_handle(File_processor::Stream::Native_handle &handle) override;

private:
    /** Completion port handle for all file handles. */
    HANDLE completion_port;
    /** Dispatcher thread for I/O completion events dispatching. */
    std::thread dispatcher_thread;

    /** Dispatcher thread function. */
    void
    Dispatcher_thread();
};

} /* namespace internal */
} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_OVERLAPPED_IO_CONTROLLER_H_ */
