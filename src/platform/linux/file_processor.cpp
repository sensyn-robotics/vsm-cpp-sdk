// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Platform-dependent part for File_processor and nested classes.
 * Linux-specific part.
 */

#include <vsm/file_processor.h>
#include <vsm/debug.h>

#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/file.h>

using namespace vsm;

namespace {

/** Linux-specific implementation for I/O controller. */
class Poll_io_controller: public File_processor::Native_controller {
public:
    struct Io_cb {
        enum class Operation {
            READ,
            WRITE
        };

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
    };
    /** Dispatcher thread for poll events dispatching. */
    std::thread dispatcher_thread;
    /** Quit request. */
    std::atomic<bool> quit_req;
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

/** POSIX-specific implementation of system native file handle. */
class Posix_file_handle: public File_processor::Stream::Native_handle {
public:
    Posix_file_handle(const std::string &path, File_processor::Stream::Mode mode,
                      File_processor::Stream::Native_handle::Envelope *handle,
                      File_processor::Stream::Native_handle::Envelope *write_handle);

    virtual
    ~Posix_file_handle();

private:
    struct Io_cb: public Poll_io_controller::Io_cb {
        Posix_file_handle &handle;

        Io_cb(Posix_file_handle &handle):
            handle(handle)
        {}
    };
    /** POSIX file descriptor. */
    int fd, write_fd = -1;
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

    Poll_io_controller &
    Get_controller() const
    {
        return reinterpret_cast<Poll_io_controller &>(Get_native_controller());
    }
};

/* ****************************************************************************/
/* Posix_file_handle class. */

Posix_file_handle::Posix_file_handle(
    const std::string &path, File_processor::Stream::Mode mode,
    File_processor::Stream::Native_handle::Envelope *handle,
    File_processor::Stream::Native_handle::Envelope *write_handle):
    read_cb(*this), write_cb(*this)
{
    if (handle) {
        fd = *reinterpret_cast<int *>(handle->Get_handle());
        if (write_handle) {
            write_fd = *reinterpret_cast<int *>(write_handle->Get_handle());
        }
        return;
    }
    int flags = 0;
    if (mode.read) {
        if (mode.extended) {
            flags = O_RDWR;
        } else {
            flags = O_RDONLY;
        }
        if (mode.should_not_exist) {
            flags |= O_CREAT;
        }
    } else if (mode.write) {
        flags = O_CREAT | O_TRUNC;
        if (mode.extended) {
            flags |= O_RDWR;
        } else {
            flags |= O_WRONLY;
        }
        if (mode.should_not_exist) {
            flags |= O_EXCL;
        }
    } else {
        ASSERT(false);
    }
    flags |= O_NONBLOCK;
    fd = open(path.c_str(), flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        switch (errno) {
        case EACCES:
            VSM_EXCEPTION(File_processor::Permission_denied_exception,
                          "Insufficient permissions for file opening: %s",
                          path.c_str());
        case EEXIST:
            VSM_EXCEPTION(File_processor::Already_exists_exception,
                          "File already exists: %s", path.c_str());
        case ENOENT:
            VSM_EXCEPTION(File_processor::Not_found_exception,
                         "File not found: %s", path.c_str());
        default:
            VSM_EXCEPTION(File_processor::Exception,
                          "Failed to open file '%s': %s", path.c_str(),
                          Log::Get_system_error().c_str());
        }
    }
}

Posix_file_handle::~Posix_file_handle()
{
    if (!is_closed) {
        Close();
    }
}

Io_result
Posix_file_handle::Map_error(int error)
{
    switch (error) {
    case 0:
        return Io_result::OK;
    case EACCES:
    case EBADF: /* Returned when FD is opened without required access. */
        return Io_result::PERMISSION_DENIED;
    case ECANCELED:
        return Io_result::CANCELED;
    default:
        return Io_result::OTHER_FAILURE;
    }
}

void
Posix_file_handle::Write()
{
    std::unique_lock<std::mutex> lock(write_mutex);

    write_cb.fd = write_fd == -1 ? fd : write_fd;
    write_cb.offset = cur_write_request->Offset();
    write_cb.buf =
        const_cast<void *>(cur_write_request->Data_buffer()->Get_data());
    write_cb.size = cur_write_request->Data_buffer()->Get_length();
    write_cb.op = Io_cb::Operation::WRITE;
    write_cb.cbk = &Posix_file_handle::Write_complete_cbk_s;

    Set_write_activity(true);

    if (!Get_controller().Queue_operation(write_cb)) {
        int error = errno;
        LOG_ERROR("Write failed: %s", Log::Get_system_error().c_str());
        cur_write_request->Set_result_arg(Map_error(error));
        cur_write_request->Complete();
        Set_write_activity(false, std::move(lock));
    }
}

void
Posix_file_handle::Write_complete_cbk_s(Poll_io_controller::Io_cb &io_cb)
{
    reinterpret_cast<Io_cb &>(io_cb).handle.Write_complete_cbk();
}

void
Posix_file_handle::Write_complete_cbk()
{
    std::unique_lock<std::mutex> lock(write_mutex);

    if (cur_write_request->Get_status() != Request::Status::PROCESSING) {
        /* Canceled, no further processing required. */
        Handle_write_abort();
        Set_write_activity(false, std::move(lock));
        return;
    }

    if (is_closed) {
        cur_write_request->Set_result_arg(Io_result::CLOSED);
        cur_write_request->Complete();
        Set_write_activity(false, std::move(lock));
        return;
    }

    int error = write_cb.error;
    ssize_t size = write_cb.return_value;

    Io_result result;
    if (size < 0 || error != 0) {
        errno = error;
        LOG_ERROR("Write failed (async): %s", Log::Get_system_error().c_str());
        result = Map_error(error);
    } else if (size < static_cast<ssize_t>(write_cb.size)) {
        /* Incomplete write, schedule the rest. */
        write_cb.size -= size;
        if (write_cb.offset != Io_stream::OFFSET_NONE) {
            write_cb.offset += size;
        }
        write_cb.buf =
            reinterpret_cast<uint8_t *>(write_cb.buf) + size;
        if (!Get_controller().Queue_operation(write_cb)) {
            int error = errno;
            LOG_ERROR("Write failed (continuation): %s",
                      Log::Get_system_error().c_str());
            result = Map_error(error);
        } else {
            /* Write for the rest data is scheduled. */
            return;
        }
    } else {
        /* Operation successfully completed. */
        result = Io_result::OK;
    }
    cur_write_request->Set_result_arg(result);
    cur_write_request->Complete();
    Set_write_activity(false, std::move(lock));
}

void
Posix_file_handle::Read()
{
    std::unique_lock<std::mutex> lock(read_mutex);

    read_cb.fd = fd;
    read_cb.offset = cur_read_request->Offset();
    read_cb.size = cur_read_request->Get_max_to_read();
    min_read_size = cur_read_request->Get_min_to_read();
    read_buf = std::make_shared<std::vector<uint8_t>>(read_cb.size);
    read_cb.buf = &read_buf->front();
    read_cb.op = Io_cb::Operation::READ;

    read_cb.cbk = &Posix_file_handle::Read_complete_cbk_s;

    Set_read_activity(true);

    if (!Get_controller().Queue_operation(read_cb)) {
        int error = errno;
        LOG_ERROR("Read failed: %s", Log::Get_system_error().c_str());
        cur_read_request->Set_result_arg(Map_error(error));
        cur_read_request->Complete();
        Set_read_activity(false, std::move(lock));
    }
}

File_processor::Stream::Lock_result
Posix_file_handle::Try_lock()
{
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        return File_processor::Stream::Lock_result::OK;
    } else {
        if (errno == EWOULDBLOCK) {
            return File_processor::Stream::Lock_result::BLOCKED;
        }
    }
    return File_processor::Stream::Lock_result::ERROR;
}

bool
Posix_file_handle::Lock()
{
    while (flock(fd, LOCK_EX) != 0) {
        if (errno != EINTR) {
            return false;
        }
    };
    return true;
}

bool
Posix_file_handle::Unlock()
{
    return (flock(fd, LOCK_UN) == 0);
}

void
Posix_file_handle::Read_complete_cbk_s(Poll_io_controller::Io_cb &io_cb)
{
    reinterpret_cast<Io_cb &>(io_cb).handle.Read_complete_cbk();
}

void
Posix_file_handle::Read_complete_cbk()
{
    std::unique_lock<std::mutex> lock(read_mutex);

    if (cur_read_request->Get_status() != Request::Status::PROCESSING) {
        /* Canceled, no further processing required. */
        Handle_read_abort();
        Set_read_activity(false, std::move(lock));
        return;
    }

    if (is_closed) {
        cur_read_request->Set_result_arg(Io_result::CLOSED);
        cur_read_request->Complete();
        Set_read_activity(false, std::move(lock));
        return;
    }

    int error = read_cb.error;
    ssize_t size = read_cb.return_value;

    Io_result result;
    if (size < 0 || error != 0) {
        errno = error;
        LOG_ERROR("Read failed (async): %s", Log::Get_system_error().c_str());
        result = Map_error(error);
    } else if (size == 0) {
        result = Io_result::END_OF_FILE;
        read_buf->resize(read_buf->size() - read_cb.size);
        cur_read_request->Set_buffer_arg(Io_buffer::Create(std::move(read_buf)));
    } else if (size < static_cast<ssize_t>(min_read_size)) {
        /* Incomplete read, schedule the rest. */
        read_cb.size -= size;
        min_read_size -= size;
        if (read_cb.offset != Io_stream::OFFSET_NONE) {
            read_cb.offset += size;
        }
        read_cb.buf = reinterpret_cast<uint8_t *>(read_cb.buf) + size;
        if (!Get_controller().Queue_operation(read_cb)) {
            int error = errno;
            LOG_ERROR("Read failed (continuation): %s",
                      Log::Get_system_error().c_str());
            result = Map_error(error);
        } else {
            /* Read for the rest data is scheduled. */
            return;
        }
    } else {
        /* Operation successfully completed. */
        result = Io_result::OK;
        read_cb.size -= size;
        read_buf->resize(read_buf->size() - read_cb.size);
        cur_read_request->Set_buffer_arg(Io_buffer::Create(std::move(read_buf)));
    }
    cur_read_request->Set_result_arg(result);
    cur_read_request->Complete();
    Set_read_activity(false, std::move(lock));
}

bool
Posix_file_handle::Cancel_write()
{
    if (is_closed) {
        return true;
    }
    std::unique_lock<std::mutex> lock(write_mutex);
    if (cur_write_request) {
        if (!Get_controller().Cancel_operation(write_cb)) {
            return false;
        }
        Set_write_activity(false, std::move(lock));
    }
    return true;
}

bool
Posix_file_handle::Cancel_read()
{
    if (is_closed) {
        return true;
    }
    std::unique_lock<std::mutex> lock(read_mutex);
    if (cur_read_request) {
        if (!Get_controller().Cancel_operation(read_cb)) {
            return false;
        }
        read_buf = nullptr;
        Set_read_activity(false, std::move(lock));
    }
    return true;
}

void
Posix_file_handle::Close()
{
    std::lock(write_mutex, read_mutex);
    if (write_active && Get_controller().Cancel_operation(write_cb)) {
        Set_write_activity(false);
    }
    if (read_active && Get_controller().Cancel_operation(read_cb)) {
        Set_read_activity(false);
    }
    is_closed = true;
    close(fd);
    if (write_fd != -1) {
        close(write_fd);
    }
    fd = -1;
    /* Hold reference to stream to prevent too early destruction. */
    auto stream_ref = stream;
    stream = nullptr;
    read_mutex.unlock();
    write_mutex.unlock();
    /* Current requests are aborted/canceled by caller. */
}

/* ****************************************************************************/
/* Epoll_io_controller class. */

Poll_io_controller::Poll_io_controller():
    quit_req(false), poll_fd_array(1)
{
    int fd[2];
    if (pipe(fd) == -1) {
        VSM_SYS_EXCEPTION("pipe() call failed");
    }
    signal_fd = fd[1];
    poll_fd_array[0].fd = fd[0];
    poll_fd_array[0].events = POLLIN;
}

Poll_io_controller::~Poll_io_controller()
{
}

void
Poll_io_controller::Enable()
{
    dispatcher_thread = std::thread(&Poll_io_controller::Dispatcher_thread,
                                    this);
}

void
Poll_io_controller::Disable()
{
    /* Signal dispatcher to exit. */
    quit_req = true;
    dispatcher_thread.join();
}

size_t
Poll_io_controller::Allocate_poll_fd_index()
{
    size_t idx;
    for (idx = 1; idx < poll_fd_array.size(); idx++) {
        if (poll_fd_array[idx].fd < 0) {
            return idx;
        }
    }
    poll_fd_array.resize(idx + 1);
    return idx;
}

void
Poll_io_controller::Release_poll_fd_index(size_t idx)
{
    poll_fd_array[idx].fd = -1;
}

void
Poll_io_controller::Send_signal()
{
    if (write(signal_fd, "", 1) != 1) {
        VSM_SYS_EXCEPTION("Signal write failed");
    }
}

void
Poll_io_controller::Accept_signal()
{
    char buf;
    if (read(poll_fd_array[0].fd, &buf, 1) != 1) {
        VSM_SYS_EXCEPTION("Signal read failed");
    }
}

void
Poll_io_controller::Update_poll_fd_array()
{
    for (auto it = fd_map.begin(); it != fd_map.end();) {
        File_desc &fd = it->second;
        if (fd.read_cb || fd.write_cb) {
            if (!fd.poll_fd_idx) {
                fd.poll_fd_idx = Allocate_poll_fd_index();
            }
            pollfd &poll_fd = poll_fd_array[fd.poll_fd_idx];
            poll_fd.fd = it->first;
            poll_fd.revents = 0;
            poll_fd.events = 0;
            if (fd.read_cb) {
                poll_fd.events |= POLLIN;
            }
            if (fd.write_cb) {
                poll_fd.events |= POLLOUT;
            }
            it++;
        } else {
            if (fd.poll_fd_idx) {
                Release_poll_fd_index(fd.poll_fd_idx);
            }
            it = fd_map.erase(it);
        }
    }
}

void
Poll_io_controller::Dispatcher_thread()
{
    while (!quit_req) {
        int n = poll(&poll_fd_array.front(), poll_fd_array.size(), 500);
        if (n < 0) {
            if (errno == EINTR) {
                /* Ignore signals. */
                continue;
            }
            VSM_SYS_EXCEPTION("poll() failed");
        }
        if (n == 0) {
            continue;
        }
        for (size_t idx = 0; n && idx < poll_fd_array.size(); idx++) {
            std::unique_lock<std::mutex> lock(map_mutex);
            pollfd &poll_fd = poll_fd_array[idx];
            if (!poll_fd.revents) {
                continue;
            }
            if (idx == 0) {
                /* Signal received from client. */
                ASSERT(poll_fd.revents == POLLIN);
                Accept_signal();
                n--;
                continue;
            }
            auto it = fd_map.find(poll_fd.fd);
            ASSERT(it != fd_map.end());
            /* May be cancelled if control block is nullptr. */
            if ((poll_fd.revents & POLLIN) && it->second.read_cb) {
                Io_cb &io_cb = *it->second.read_cb;
                it->second.read_cb = nullptr;
                Seek(io_cb);
                lock.unlock();
                Read(io_cb);
            } else if ((poll_fd.revents & POLLOUT) && it->second.write_cb) {
                /* Only one operation in one pass. Another will be done after
                 * next poll() call.
                 */
                Io_cb &io_cb = *it->second.write_cb;
                it->second.write_cb = nullptr;
                Seek(io_cb);
                lock.unlock();
                Write(io_cb);
            }
        }
        std::unique_lock<std::mutex> lock(map_mutex);
        Update_poll_fd_array();
    }
}

void
Poll_io_controller::Seek(Io_cb &io_cb)
{
    if (io_cb.offset == Io_stream::OFFSET_NONE) {
        return;
    }
    off_t result;
    if (io_cb.offset == Io_stream::OFFSET_END) {
        result = lseek(io_cb.fd, 0, SEEK_END);
    } else {
        result = lseek(io_cb.fd, io_cb.offset, SEEK_SET);
    }
    if (result == -1) {
        VSM_SYS_EXCEPTION("lseek failed");
    }
    io_cb.offset = result;
}

void
Poll_io_controller::Read(Io_cb &io_cb)
{
    io_cb.return_value = read(io_cb.fd, io_cb.buf, io_cb.size);
    io_cb.error = io_cb.return_value == -1 ? errno : 0;
    if (io_cb.cbk) {
        io_cb.cbk(io_cb);
    }
}

void
Poll_io_controller::Write(Io_cb &io_cb)
{
    io_cb.return_value = write(io_cb.fd, io_cb.buf, io_cb.size);
    io_cb.error = io_cb.return_value == -1 ? errno : 0;
    if (io_cb.cbk) {
        io_cb.cbk(io_cb);
    }
}

bool
Poll_io_controller::Queue_operation(Io_cb &io_cb)
{
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = fd_map.find(io_cb.fd);
    if (it == fd_map.end()) {
        it = fd_map.emplace(io_cb.fd, File_desc()).first;
    }
    File_desc &fd = it->second;
    if (io_cb.op == Io_cb::Operation::READ) {
        ASSERT(!fd.read_cb);
        fd.read_cb = &io_cb;
    } else {
        ASSERT(io_cb.op == Io_cb::Operation::WRITE);
        ASSERT(!fd.write_cb);
        fd.write_cb = &io_cb;
    }
    Send_signal();
    return true;
}

bool
Poll_io_controller::Cancel_operation(Io_cb &io_cb)
{
    std::lock_guard<std::mutex> lock(map_mutex);
    auto it = fd_map.find(io_cb.fd);
    if (it == fd_map.end()) {
        return false;
    }
    if (io_cb.op == Io_cb::Operation::READ) {
        if (!it->second.read_cb) {
            return false;
        }
        it->second.read_cb = nullptr;
    } else {
        ASSERT(io_cb.op == Io_cb::Operation::WRITE);
        if (!it->second.write_cb) {
            return false;
        }
        it->second.write_cb = nullptr;
    }
    Send_signal();
    return true;
}

} /* anonymous namespace */

std::unique_ptr<File_processor::Stream::Native_handle>
File_processor::Stream::Native_handle::Create(const std::string &path, Mode mode,
                                              Envelope *handle,
                                              Envelope *write_handle)
{
    return std::unique_ptr<Native_handle>
        (new Posix_file_handle(path, mode, handle, write_handle));
}

std::unique_ptr<File_processor::Native_controller>
File_processor::Native_controller::Create()
{
    return std::unique_ptr<Native_controller>(new Poll_io_controller());
}

