// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file io_stream.h
 *
 * Abstract stream for I/O operations. Used as a back-end for already created
 * streams.
 */
#ifndef _IO_STREAM_H_
#define _IO_STREAM_H_

#include <ugcs/vsm/io_buffer.h>
#include <ugcs/vsm/request_temp_completion_context.h>
#include <ugcs/vsm/operation_waiter.h>
#include <ugcs/vsm/reference_guard.h>

#include <chrono>

namespace ugcs {
namespace vsm {

/** Result of I/O operation. */
enum class Io_result {
    /** Operation completed successfully. */
    OK,
    /** Operation timed out. */
    TIMED_OUT,
    /** Operation canceled. */
    CANCELED,
    /** Bad address specific. It could not be understood and used. */
    BAD_ADDRESS,
    /** Remote side has explicitly refused the connection. */
    CONNECTION_REFUSED,
    /** Stream has been or is closed. All pending or new operations
     * initiated for closed stream are completed with this result. */
    CLOSED,
    /** Insufficient permissions for the requested operation. */
    PERMISSION_DENIED,
    /** End of file encountered. */
    END_OF_FILE,
    /** File locking error. Possible double lock or unlock while not locked*/
    LOCK_ERROR,
    /** Some other system failure. If happened, it is recommended to
     * investigate the root cause. */
    OTHER_FAILURE
};

/** Abstract I/O stream interface. All SDK objects which supports reading
 * and/or writing raw bytes (like network connections, files, serial
 * connections) implement this interface.
 */
class Io_stream: public std::enable_shared_from_this<Io_stream> {
    DEFINE_COMMON_CLASS(Io_stream, Io_stream)
public:
    /** Guard object. */
    typedef Reference_guard<Io_stream::Ptr> Ref;

    /** Offset for read/write operations. Used by streams which support
     * offsets (like files). */
    typedef int64_t Offset;
    /** Offset special value which indicates that the offset value is
     * not specified.
     */
    static const Offset OFFSET_NONE;
    /** Offset special value which indicates that the offset value
     * corresponds to the stream end (e.g. append operation).
     */
    static const Offset OFFSET_END;

    /** Default prototype for write operation completion handler. */
    typedef Callback_proxy<void, Io_result> Write_handler;

    /** Default prototype for read operation completion handler. */
    typedef Callback_proxy<void, Io_buffer::Ptr, Io_result> Read_handler;

    /** Default prototype for close operation completion handler. */
    typedef Callback_proxy<void> Close_handler;

    /** Stream states. */
    enum class State {
        /** Stream is closed. Also initial state. */
        CLOSED,
        /** Stream is being opened and is not yet ready for reading/writing.
         * This process may fail. */
        OPENING,
        /** Stream is being passively opened and is not yet ready for reading/writing. (listening socket)
         * This process may fail. */
        OPENING_PASSIVE,
        /** Stream is opened and is ready for read/write operations. */
        OPENED
    };

    /** Stream types. */
    enum class Type {
        FILE,
        SERIAL,
        ANDROID_SERIAL,
        TCP,
        UDP,
        UDP_MULTICAST,
        CAN,
        UNDEFINED
    };

    virtual
    ~Io_stream()
    {}

    /** Add reference to the stream. */
    void
    Add_ref();

    /** Release reference for the stream. The stream is closed when last
     * reference is released.
     */
    void
    Release_ref();

    /** Initiate write operation.
     * @param buffer Buffer with data to write.
     * @param offset Write offset in the stream if supported. Value
     *      OFFSET_NONE indicates that stream-maintained offset should be
     *      used.
     * @param completion_handler Handler to invoke when the operation is
     *      completed.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     * @throw Invalid_param_exception If handler is set without completion
     * context or vice versa.
     */
    Operation_waiter
    Write(Io_buffer::Ptr buffer,
          Offset offset,
          Write_handler completion_handler = Make_dummy_callback<void, Io_result>(),
          Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        if (!!completion_handler != !!comp_ctx) {
            VSM_EXCEPTION(Invalid_param_exception, "Completion handler can not "
                    "exist without completion context and vice versa.");
        }
        return Write_impl(buffer, offset, completion_handler, comp_ctx);
    }

    /** Initiate write operation.
     * @param buffer Buffer with data to write.
     * @param completion_handler Handler to invoke when the operation is
     *      completed.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     * @throw Invalid_param_exception If handler is set without completion
     * context or vice versa.
     */
    Operation_waiter
    Write(Io_buffer::Ptr buffer,
          Write_handler completion_handler = Make_dummy_callback<void, Io_result>(),
          Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        if (!!completion_handler != !!comp_ctx) {
            VSM_EXCEPTION(Invalid_param_exception, "Completion handler can not "
                    "exist without completion context and vice versa.");
        }
        return Write_impl(buffer, OFFSET_NONE, completion_handler, comp_ctx);
    }

    /** Initiate read operation.
     * @param max_to_read Maximal number of bytes to read from the stream.
     *      Less bytes can be read in fact.
     * @param min_to_read Minimal number of bytes to read from the stream. The
     *      operation is not completed until the specified minimal number of
     *      bytes is read.
     * @param offset Read offset in the stream if supported. Value
     *      OFFSET_NONE indicates that stream-maintained offset should be
     *      used.
     * @param completion_handler Handler to invoke when the operation is
     *      completed.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     * @throw Invalid_param_exception If handler is set without completion
     * context or vice versa.
     * @throw Invalid_param_exception If Max to read is less then Min to read.
     */
    Operation_waiter
    Read(size_t max_to_read, size_t min_to_read, Offset offset,
         Read_handler completion_handler = Make_dummy_callback<void, Io_buffer::Ptr, Io_result>(),
         Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        if (!!completion_handler != !!comp_ctx) {
            VSM_EXCEPTION(Invalid_param_exception, "Completion handler can not "
                    "exist without completion context and vice versa.");
        }
        if (max_to_read < min_to_read) {
            VSM_EXCEPTION(Invalid_param_exception, "max_to_read cannot be less than min_to_read");
        }
        return Read_impl(max_to_read, min_to_read, offset, completion_handler,
                         comp_ctx);
    }

    /** Initiate read operation.
     * @param max_to_read Maximal number of bytes to read from the stream.
     *      Less bytes can be read in fact.
     * @param min_to_read Minimal number of bytes to read from the stream. The
     *      operation is not completed until the specified minimal number of
     *      bytes is read.
     * @param completion_handler Handler to invoke when the operation is
     *      completed.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     * @throw Invalid_param_exception If handler is set without completion
     * context or vice versa.
     * @throw Invalid_param_exception If Max to read is less then Min to read.
     */
    Operation_waiter
    Read(size_t max_to_read, size_t min_to_read = 1,
         Read_handler completion_handler = Make_dummy_callback<void, Io_buffer::Ptr, Io_result>(),
         Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        if (!!completion_handler != !!comp_ctx) {
            VSM_EXCEPTION(Invalid_param_exception, "Completion handler can not "
                    "exist without completion context and vice versa.");
        }
        if (max_to_read < min_to_read) {
            VSM_EXCEPTION(Invalid_param_exception, "max_to_read cannot be less than min_to_read");
        }
        return Read_impl(max_to_read, min_to_read, OFFSET_NONE, completion_handler, comp_ctx);
    }

    /** Initiate stream close operation.
     * @param completion_handler Completion handler for the operation.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     * @throw Invalid_param_exception If handler is set without completion
     * context or vice versa.
     */
    Operation_waiter
    Close(Close_handler completion_handler = Make_dummy_callback<void>(),
          Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        if (!!completion_handler != !!comp_ctx) {
            VSM_EXCEPTION(Invalid_param_exception, "Completion handler can not "
                    "exist without completion context and vice versa.");
        }
        return Close_impl(completion_handler, comp_ctx);
    }

    /** There is no sense in copying the stream. Multiple users of the same
     * stream should copy the shared pointer Ptr.
     */
    Io_stream(const Io_stream&) = delete;

    /**
     * Constructor.
     */
    Io_stream(Type type):
        stream_type(type)
    {}

    /** Get current state of the stream. */
    State
    Get_state() const
    {
        return state;
    }

    /** Checks if stream is closed or not. */
    bool
    Is_closed() const
    {
        return (state == State::CLOSED);
    }

    /** Get human readable stream name. */
    std::string
    Get_name() const;

    Type
    Get_type() const
    {
        return stream_type;
    }

    /** Convert Io_result value to character string. */
    static const char*
    Io_result_as_char(const Io_result res);

protected:
    Type stream_type;

    /** Current state of the stream. */
    State state = State::CLOSED;
    /** Reference counter. */
    std::atomic_int ref_count = { 0 };

    /** Write call implementation.
     * @param buffer Buffer with data to write.
     * @param offset Write offset in the stream if supported. Value
     *      OFFSET_NONE indicates that stream-maintained offset should be
     *      used.
     * @param completion_handler Handler to invoke when the operation is
     *      completed.
     * @param comp_ctx Completion context for the operation.
     * @return Waiter object which can be used for synchronization and control.
     */
    virtual Operation_waiter
    Write_impl(Io_buffer::Ptr buffer, Offset offset,
               Write_handler completion_handler,
               Request_completion_context::Ptr comp_ctx) = 0;

    /** Read call implementation.
     * @param max_to_read Maximal number of bytes to read.
     * @param min_to_read Minimal number of bytes to read.
     * @param offset Operation offset.
     * @param completion_handler Completion handler.
     * @param comp_ctx Completion context.
     * @return Operation waiter.
     *
     * @see Read
     */
    virtual Operation_waiter
    Read_impl(size_t max_to_read, size_t min_to_read, Offset offset,
              Read_handler completion_handler,
              Request_completion_context::Ptr comp_ctx) = 0;

    /** Close call implementation.
     * @param completion_handler Completion handler.
     * @param comp_ctx Completion context.
     * @return Operation waiter.
     *
     * @see Close
     */
    virtual Operation_waiter
    Close_impl(Close_handler completion_handler,
               Request_completion_context::Ptr comp_ctx) = 0;

    /** Set the stream name. */
    void
    Set_name(const std::string&);

private:

    /** Name mutex, global for all streams, because operations with name
     * are rare.
     */
    static std::mutex name_mutex;

    /** Human readable stream name. */
    std::string name = "[undefined]";
};

/** Convenience builder for write operation callbacks. */
DEFINE_CALLBACK_BUILDER(Make_write_callback, (Io_result), (Io_result::OTHER_FAILURE))

/** Convenience builder for read operation callbacks. */
DEFINE_CALLBACK_BUILDER(Make_read_callback, (Io_buffer::Ptr, Io_result),
                        (nullptr, Io_result::OTHER_FAILURE))

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _IO_STREAM_H_ */
