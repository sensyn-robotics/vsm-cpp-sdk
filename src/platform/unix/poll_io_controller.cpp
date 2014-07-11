#include <ugcs/vsm/poll_io_controller.h>

using namespace ugcs::vsm::internal;

Poll_io_controller::Poll_io_controller() :
        poll_fd_array(1)
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
