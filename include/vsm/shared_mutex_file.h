// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file shared_mutex_file.h
 */

#ifndef SHARED_MUTEX_FILE_H_
#define SHARED_MUTEX_FILE_H_

#include <vsm/callback.h>
#include <vsm/file_processor.h>

namespace vsm {

/** Cross-platform named mutex. */
class Shared_mutex_file: public std::enable_shared_from_this<Shared_mutex_file>
{
    DEFINE_COMMON_CLASS(Shared_mutex_file, Shared_mutex_file)
public:

    /**
     * Acquire Callback prototype.
     * void Callback(result, void* mem_pointer, ...)
     * @param result See Acquire_result above
     * @param mem_pointer Pointer to memory region. If the mem_pointer is not
     *        nullptr then user can freely access mem_size count bytes. User is
     *        free to use the memory from any thread.
     * @param ... Any other user supplied arguments
     *
     * It is ok to call Release() from this function, too.
     */
    typedef File_processor::Stream::Lock_handler Acquire_handler;

    /**
     * Creates shared memory region which can be accessed via Acquire call.
     * @param name Name of shared memory region.
     */
    Shared_mutex_file(const std::string& name, File_processor::Ptr = File_processor::Get_instance());

    virtual ~Shared_mutex_file();

    /** Builder for acquire handler. */
    DEFINE_CALLBACK_BUILDER (
            Make_acquire_handler,
            (Io_result),
            (Io_result::OTHER_FAILURE));

    /**
     * Acquires exclusive lock
     *
     * @param completion_handler User supplied callback. See above.
     * @param comp_ctx User supplied completion context in which completion_handler
     *                 will execute.
     * @return Operation_waiter Can be used to acquire memory synchronously
     *         or with timeout.
     *
     *         IMPORTANT:
     *         Operation_waiter timoeut or cancelation DOES NOT
     *         prevent completion_handler from being called.
     */
    Operation_waiter
    Acquire(Acquire_handler completion_handler,
            Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());

    /**
     * Releases previously acquired lock.
     */
    Operation_waiter
    Release(Acquire_handler completion_handler = Make_dummy_callback<void, Io_result>(),
            Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());
private:
    File_processor::Stream::Ref stream;
};

} /* namespace vsm */
#endif /* SHARED_MUTEX_FILE_H_ */
