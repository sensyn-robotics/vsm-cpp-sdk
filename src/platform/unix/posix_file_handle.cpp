#include <ugcs/vsm/posix_file_handle.h>
#include <sys/file.h>

using namespace ugcs::vsm::internal;
using namespace ugcs::vsm;

Posix_file_handle::Posix_file_handle(int fd, int write_fd):
        fd(fd), write_fd(write_fd), read_cb(*this), write_cb(*this)
{
}

Posix_file_handle::Posix_file_handle(
        const std::string& path,
        const File_processor::Stream::Mode& mode,
        mode_t permissions): read_cb(*this), write_cb(*this)
{
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
    fd = open(path.c_str(), flags, permissions);
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
    Read_impl(std::move(lock));
}

void
Posix_file_handle::Read_impl(std::unique_lock<std::mutex>&& read_lock)
{
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
        Set_read_activity(false, std::move(read_lock));
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
    std::unique_lock<std::mutex> lock(write_mutex);
    if (cur_write_request) {
        if (is_closed) {
            /* Cancel attempt was already made during closing, write_active
             * variable indicates whether that cancel was successful or not. */
            return !write_active;
        }
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
    std::unique_lock<std::mutex> lock(read_mutex);
    if (cur_read_request) {
        if (is_closed) {
            /* Cancel attempt was already made during closing, read_active
             * variable indicates whether that cancel was successful or not. */
            return !read_active;
        }
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
    Get_controller().Delete_handle(fd);
    if (write_fd != -1) {
        Get_controller().Delete_handle(write_fd);
        write_fd = -1;
    }
    fd = -1;
    /* Hold reference to stream to prevent too early destruction. */
    auto stream_ref = stream;
    stream = nullptr;
    read_mutex.unlock();
    write_mutex.unlock();
    /* Current requests are aborted/canceled by caller. */
}
