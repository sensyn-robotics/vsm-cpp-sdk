// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file posix_file_handle.h
 */
#ifndef _POSIX_FILE_HANDLE_H_
#define _POSIX_FILE_HANDLE_H_

#include <ugcs/vsm/poll_io_controller.h>
#include <ugcs/vsm/platform_sockets.h>

namespace ugcs {
namespace vsm {
namespace internal {

/** POSIX-specific implementation of system native file handle. */
class Posix_file_handle:
        public ugcs::vsm::File_processor::Stream::Native_handle {

public:

    /** Construct an instance based on already opened descriptors. */
    Posix_file_handle(int fd, int write_fd = -1);

    /** Construct an instance by opening a file specified by path. */
    Posix_file_handle(
            const std::string& path,
            const File_processor::Stream::Mode& mode,
            mode_t permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    /** Closes descriptors on destruction. */
    virtual
    ~Posix_file_handle();

protected:

    /** Posix specific I/O control block. */
    struct Io_cb: public Poll_io_controller::Io_cb {

        /** Construct block bound to a handle. */
        Io_cb(Posix_file_handle &handle):
            handle(handle)
        {}

        /** Associated handle. */
        Posix_file_handle &handle;
    };

    /** POSIX file descriptor for read/write operations. */
    int fd = -1;
    /** Optional separate descriptor for write operations. */
    int write_fd = -1;
    /** Control block for current read operation. */
    Io_cb read_cb;
    /** Control block for current write operation. */
    Io_cb write_cb;
    /** Read buffer. */
    std::shared_ptr<std::vector<uint8_t>> read_buf;
    /** Mutex for protecting write control block. */
    std::mutex write_mutex,
    /** Mutex for protecting read control block. */
    read_mutex;
    /** Minimal number of bytes to read in current read operation. */
    size_t min_read_size;

    /** Schedule write operation based on current write request. */
    virtual void
    Write() override;

    /** Callback used as completion handler for platform write call. */
    static void
    Write_complete_cbk_s(Poll_io_controller::Io_cb &io_cb);

    /** Completion handler for platform write call. */
    void
    Write_complete_cbk();

    /** Schedule read operation based on current read request. */
    virtual void
    Read() override;

    /** Implementation of read operation. */
    void
    Read_impl(std::unique_lock<std::mutex>&& read_lock);

    /** Schedule lock operation based on current lock request. */
    virtual File_processor::Stream::Lock_result
    Try_lock() override;

    /** Schedule lock operation based on current lock request. */
    virtual bool
    Lock() override;

    /** Schedule unlock operation based on current lock request. */
    virtual bool
    Unlock() override;

    /** Callback used as completion handler for platform read call. */
    static void
    Read_complete_cbk_s(Poll_io_controller::Io_cb &io_cb);

    /** Completion handler for platform read call. */
    void
    Read_complete_cbk();

    /** Cancel current write operation. */
    virtual bool
    Cancel_write() override;

    /** Cancel current read operation. */
    virtual bool
    Cancel_read() override;

    /** Close the handle. */
    virtual void
    Close() override;

    /** Map errno value to Io_result. */
    static Io_result
    Map_error(int error);

    /** Get poll controller. */
    Poll_io_controller &
    Get_controller() const
    {
        return reinterpret_cast<Poll_io_controller &>(Get_native_controller());
    }

};

} /* namespace internal */
} /* namespace vsm */
} /* namespace ugcs */

#endif /* _POSIX_FILE_HANDLE_H_ */
