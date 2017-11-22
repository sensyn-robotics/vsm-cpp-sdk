// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file poll_io_controller.h
 */
#ifndef _POLL_IO_CONTROLLER_H_
#define _POLL_IO_CONTROLLER_H_

#include <ugcs/vsm/file_processor.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

namespace ugcs {
namespace vsm {
namespace internal {

/** Linux-specific implementation for I/O controller. */
class Poll_io_controller: public File_processor::Native_controller {
public:

    /** Control block for I/O operation. */
    struct Io_cb {
        /** Operation type. */
        // @{
        enum class Operation {
            READ,
            WRITE
        };
        // @}

        /** Callback for completed operation. */
        typedef void (*Callback)(Io_cb &);

        /** File descriptor. */
        int fd;
        /** Requested operation. */
        Operation op;
        /** Data buffer. */
        void *buf;
        /** Data size. */
        size_t size;
        /** File offset. */
        Io_stream::Offset offset;
        /** Callback to call when operation is completed. */
        Callback cbk;

        /** Operation error code. */
        int error;
        /** Operation return value, typically transfer size. */
        ssize_t return_value;
    };

    Poll_io_controller();

    virtual
    ~Poll_io_controller();

    /** Enable the controller. */
    virtual void
    Enable() override;

    /** Disable the controller. */
    virtual void
    Disable() override;

    /** Register new opened file handle. */
    virtual void
    Register_handle(File_processor::Stream::Native_handle &) override
    {}

    /** Unregister previously registered file handle. */
    virtual void
    Unregister_handle(File_processor::Stream::Native_handle &) override
    {}

    /** Delete handle. Possibly deferred.
     * This is a workaround of kernel panic bug in OSX.
     * The scenario:
     * 1) create pipe with descriptors p1, p2.
     * 2) open serial port with descriptor fd.
     * 3) poll on two descriptors p1 and fd in separate thread.
     * 4) issue write on p2. This triggers read event on p1 and wakes up poll().
     * 5) close(fd)
     *
     * Results:
     *
     *   - When (4) and (5) are close enough poll does not wake up and
     *     close(fd) blocks and triggers kernel panic. This does not happen
     *     100% of time but if the above code is put into loop it
     *     takes ~20-100 iterations to trigger.
     *
     *   - When (4) and (5) are >10ms apart the poll wakes up and close succeeds.
     *
     *   - If close(fd) is called before write(p2) then I could not
     *     repeat the kernel panic.
     * Solution:
     *
     * Do not close the fd while it is in polling state.
     * Close it after poll returns.
     */
    void
    Delete_handle(int fd);

    /** Queue IO operation. The provided callback is called when the operation
     * completes with Io_cb structure filled.
     * @return True if succeeded, false otherwise. Check errno for error code.
     */
    bool
    Queue_operation(Io_cb &io_cb);

    /** Cancel pending operation.
     * @param io_cb Operation control block.
     * @return True if cancelled, false if not cancelled (e.g. too late).
     */
    bool
    Cancel_operation(Io_cb &io_cb);

private:
    /** Object represents file descriptor registered in epoll. The controller
     * supports only one read and one write operation simultaneously (for
     * simplicity because a stream serializes them anyway).
     */
    class File_desc {
    public:
        Io_cb *read_cb = nullptr,
              *write_cb = nullptr;
        /** Allocated index in poll file descriptors array. */
        size_t poll_fd_idx = 0;
        bool close_on_remove = false;
    };
    /** Dispatcher thread for poll events dispatching. */
    std::thread dispatcher_thread;
    /** Quit request. */
    std::atomic_bool quit_req = { false };
    /** Active file descriptors. */
    std::map<int, File_desc> fd_map;
    /** Mutex for map access. */
    std::mutex map_mutex;
    /** File descriptors array for poll. The first element is signal pipe. */
    std::vector<pollfd> poll_fd_array;
    /** File descriptor used to signal dispatcher about new requests. */
    int signal_fd;

    /** Dispatcher thread function. */
    void
    Dispatcher_thread();

    /** Perform read operation when event fired. */
    void
    Read(Io_cb &io_cb);

    /** Perform write operation when event fired. */
    void
    Write(Io_cb &io_cb);

    /** Apply operation offset if any. */
    void
    Seek(Io_cb &io_cb);

    size_t
    Allocate_poll_fd_index();

    void
    Release_poll_fd_index(size_t idx);

    void
    Send_signal();

    void
    Accept_signal();

    /** Should be called with map_mutex locked. */
    void
    Update_poll_fd_array();
};

} /* namespace internal */
} /* namespace vsm */
} /* namespace ugcs */

#endif /* _POLL_IO_CONTROLLER_H_ */
