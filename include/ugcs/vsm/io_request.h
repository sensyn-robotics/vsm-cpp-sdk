// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file io_request.h
 *
 * I/O request declaration.
 */

#ifndef IO_TASK_H_
#define IO_TASK_H_

#include <ugcs/vsm/io_stream.h>
#include <ugcs/vsm/utils.h>

namespace ugcs {
namespace vsm {

/** Base request for I/O operations. */
class Io_request: public Request {
    DEFINE_COMMON_CLASS(Io_request, Request)

public:

    /** Construct I/O request.
     * @param stream Related I/O stream instance.
     * @param offset Offset for I/O operation if applicable. If the target
     *      channel does not support the seeking this parameter is ignored.
     *      If the stream is responsible for keeping current offset it
     *      should check real offset when request is handled.
     * @param result_arg Reference to result value which is passed as
     *      argument to the user provided completion handler.
     */
    Io_request(Io_stream::Ptr stream, Io_stream::Offset offset,
            Io_result& result_arg):
        stream(stream), offset(offset), result_arg(result_arg)
    {}

    /** Locks and gets the associated stream. */
    Io_stream::Ptr
    Get_stream() const
    {
        return stream;
    }

    /** Get I/O operation offset value. */
    Io_stream::Offset &
    Offset()
    {
        return offset;
    }

    /** Sets the result argument.
     * @param result Resutl value to set.
     * @param lock If present, should be the already acquired lock for the
     * request (will not be unlocked by the method), otherwise new lock is
     * acquired and released inside the method.
     */
    void
    Set_result_arg(
            Io_result result,
            const Request::Locker& lock = Request::Locker())
    {
        Request::Locker local_lock;
        if (lock) {
            ASSERT(Lock(false).mutex() == lock.mutex());
        } else {
            local_lock = Lock();
        }
        if (Is_completion_handler_present()) {
            result_arg = result;
        }
        last_result = result;
    }

    /**
     * Get the most recently set result value.
     * XXX Return Io_result::OTHER_FAILURE if the result value was never set.
     */
    Io_result
    Get_last_result()
    {
        return last_result;
    }

private:
    /** Associated stream. */
    Io_stream::Ptr stream;

    /** I/O operation offset if necessary. */
    Io_stream::Offset offset;

    /** Mandatory argument of all I/O completion handlers. */
    Io_result &result_arg;

    /** The most recent result. */
    Io_result last_result = Io_result::OTHER_FAILURE;

    virtual void
    Destroy() override
    {
        stream = nullptr;
    }
};

/** Baser I/O write request. */
class Write_request : public Io_request
{
public:
    /** Shared pointer to write request. */
    typedef std::shared_ptr<Write_request> Ptr;

    /** Construct write request.
     * @param buffer Buffer with data to write.
     * @param args Arguments pack for {@link Io_request} constructor.
     */
    template <typename... Args>
    Write_request(Io_buffer::Ptr buffer, Args &&... args):
        Io_request(std::forward<Args>(args)...), buffer(buffer)
    {}

    /** Create an instance of the request. */
    template <typename... Args>
    static Ptr
    Create(Args &&... args)
    {
        return std::make_shared<Write_request>(std::forward<Args>(args)...);
    }

    /** Access the buffer with data to write. */
    Io_buffer::Ptr &
    Data_buffer()
    {
        return buffer;
    }

private:
    /** Buffer with data to write.  */
    Io_buffer::Ptr buffer;
};

/** Basic I/O read request. */
class Read_request : public Io_request
{
public:
    /** Shared pointer to read request. */
    typedef std::shared_ptr<Read_request> Ptr;

    /**
     * Constructor.
     * @param buffer_arg Reference to the I/O buffer argument of the
     * completion handler. Resulting data should be read into this buffer.
     * @param max_to_read Maximal number of bytes to read.
     * @param min_to_read Minimal number of bytes to read.
     * @param args Argument for the base class constructor.
     */
    template <typename... Args>
    Read_request(Io_buffer::Ptr& buffer_arg, size_t max_to_read, size_t min_to_read,
                 Args &&... args):
        Io_request(std::forward<Args>(args)...),
        buffer_arg(buffer_arg), max_to_read(max_to_read), min_to_read(min_to_read)
    {}

    /** Create an instance of read request. */
    template <typename... Args>
    static Ptr
    Create(Args &&... args)
    {
        return std::make_shared<Read_request>(std::forward<Args>(args)...);
    }

    /** Sets the buffer argument.
     * @param buffer Buffer to set.
     * @param lock If present, should be the already acquired lock for the
     * request (will not be unlocked by the method), otherwise new lock is
     * acquired and released inside the method.
     */
    void
    Set_buffer_arg(
            Io_buffer::Ptr buffer,
            const Request::Locker& lock = Request::Locker())
    {
        Request::Locker local_lock;
        if (lock) {
            ASSERT(Lock(false).mutex() == lock.mutex());
        } else {
            local_lock = Lock();
        }
        if (Is_completion_handler_present()) {
            buffer_arg = buffer;
        }
        last_read_buffer = buffer;
    }

    /**
     * Get the most recently set read buffer.
     * XXX Return nullptr if the read buffer was never set.
     */
    Io_buffer::Ptr
    Get_last_read_buffer()
    {
        return last_read_buffer;
    }

    /** Get maximal number of bytes to read. */
    size_t
    Get_max_to_read() const
    {
        return max_to_read;
    }

    /** Get minimal number of bytes to read. */
    size_t
    Get_min_to_read() const
    {
        return min_to_read;
    }

private:
    /** Reference to the completion handler buffer argument. */
    Io_buffer::Ptr &buffer_arg;

    /** The most recently set read buffer. */
    Io_buffer::Ptr last_read_buffer;

    /** Maximal number of bytes to read. */
    size_t max_to_read,
    /** Minimal number of bytes to read. */
           min_to_read;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* IO_TASK_H_ */
