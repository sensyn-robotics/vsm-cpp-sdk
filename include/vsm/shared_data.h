// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_data.h
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#ifndef SHARED_DATA_H_
#define SHARED_DATA_H_

#include <vsm/utils.h>
#include <vsm/callback.h>
#include <vsm/request_temp_completion_context.h>
#include <vsm/operation_waiter.h>
#include <vsm/shared_memory.h>
#include <vsm/shared_semaphore.h>

namespace vsm
{
/**
 * Implements interlocked memory region which can be used by multiple processes.
 *
 * Features:
 *  - Asynchronous acquire.
 *     Acquiring thread does not need to block if memory is locked.
 *     The user supplied callback will be called when memory is locked or error occurs.
 *
 *  - Crash/deadlock resistant.
 *     The Acquire will succeed even if memory is blocked and the process which owns
 *     the lock has terminated unexpectedly. In that case callback will be called with
 *     ACQUIRE_RESULT_OK_RECOVERED result.
 *
 *  - Thread safe Acquire/Release.
 *     The Acquire/Release functions can be called from any thread.
 *     Although user must make sure that Release() is always called after Acquire().
 *
 */
class Shared_data: public std::enable_shared_from_this<Shared_data>
{
    DEFINE_COMMON_CLASS(Shared_data, Shared_data)
public:
    /** Base class for all Shared_data exceptions. */
    VSM_DEFINE_EXCEPTION(Exception);

    /** Result codes returned via Acquire callback */
    typedef enum
    {
        /** Memory acquired successfully.
         * Release() must be called after this result.*/
        ACQUIRE_RESULT_OK,

        /** Memory acquired successfully and this is the first ever lock on it.
         * Release() must be called after this result.*/
        ACQUIRE_RESULT_OK_CREATED,

        /** Memory acquired successfully but it is in unknown state due to other
         * client has acquired it and crashed.
         * User should check the memory state and reinitilize it if needed.
         * And should call Release(true) to tell other clients that memory is ok.
         * Release() must be called after this result.*/
        ACQUIRE_RESULT_OK_RECOVERED,

        /** Memory cannot be acquired twice by the one Shared_data instance.
         * It is ok not to call Release() after this result.*/
        ACQUIRE_RESULT_ALREADY_ACQUIRED,

        /** Memory cannot be acquired because there are too many instances already
         * trying to acquire the data.
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_TOO_MANY_CLIENTS,

        /** Release() called before memory got acquired.
         * Possible causes:
         *  - Another client detected timeout for my heartbeat and
         *    released lock on my behalf.
         *  - Release called before lock could be acquired.
         *  - Operation was canceled before lock could be acquired.
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_RELEASED,

        /** User canceled the acquire via Operation_waiter::Cancel()
         *  - Operation was canceled before lock could be acquired.
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_CANCELED,

        /** Acquire() timeouted
         *  - Operation timeouted before lock could be acquired.
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_TIMEOUT,

        /** Acquire process was stopped because object Shared_memory instance is
         *  being destroyed.
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_DESTROYED,

        /** Internal error
         * It is ok not to call Release() after this result. */
        ACQUIRE_RESULT_ERROR,
    } Acquire_result;

    /**
     * Acquire Callback prototype.
     * void Callback(result, void* mem_pointer, ...)
     * @param result See Acquire_result above
     * @param mem_pointer Pointer to memory region. If the mem_pointer is not
     *        nullptr then user can freely access mem_size count bytes. User is
     *        free to use the memory from any thread.
     * @param ... Any other user supplied arguments
     *
     * User _must_ call Release() if result is one of these values:
     *   - ACQUIRE_RESULT_OK
     *   - ACQUIRE_RESULT_OK_CREATED
     *   - ACQUIRE_RESULT_OK_RECOVERED
     *
     * It is ok to call Release() from this function, too.
     */
    typedef Callback_proxy<void, Acquire_result, void*> Acquire_handler;

    /**
     * Creates shared memory region which can be accessed via Acquire call.
     * @param name Name of shared memory region.
     * @param mem_size Memory size in bytes. Can be 0. It that case the object can be
     *                 used to synchronize access to some other resource.
     * @param align Byte count to align mem_pointer. Returned mem_pointer will be
     *              aligned to this size. Should be a power of 2.
     */
    Shared_data(const std::string& name, const size_t mem_size, const size_t align = 16);

    virtual
    ~Shared_data();

    /** Builder for acquire handler. */
    DEFINE_CALLBACK_BUILDER (
            Make_acquire_handler,
            (Acquire_result, void*),
            (ACQUIRE_RESULT_ERROR, nullptr));

    /**
     * Acquires exclusive lock on shared memory region
     *
     * @param completion_handler User supplied callback. See above.
     * @param comp_ctx User supplied completion context in which completion_handler
     *                 will execute.
     * @return Operation_waiter Can be used to acquire memory synchronously
     *         or with timeout.
     *
     *         IMPORTANT:
     *         Operation_waiter timeout or cancellation DOES NOT
     *         prevent completion_handler from being called.
     */
    Operation_waiter
    Acquire(Acquire_handler completion_handler,
            Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());

    /**
     * Releases previously acquired lock.
     * @param data_valid When releasing memory user must tell whether the data
     *                   is consistent. Should be used when Acquire returned
     *                   ACQUIRE_RESULT_OK_RECOVERED.
     *                   true: Memory is consistent and usable by other clients.
     *                   false: Memory is inconsistent and I was not able to recover.
     */
    void
    Release(bool data_valid = true);

private:
    /** Shared metadata must not be locked longer than for 1 second.
     * Most likely the process which was updating metadata was terminated while doing that.
     */
    constexpr static int METADATA_TIMEOUT_MS = 1000;

    /** */
    constexpr static int METADATA_TIMEOUT_COUNT_MAX = 3;
    constexpr static std::chrono::milliseconds METADATA_TIMEOUT =
            std::chrono::milliseconds(METADATA_TIMEOUT_MS);
    constexpr static std::chrono::milliseconds HEARTBEAT_TIMEOUT =
            std::chrono::milliseconds(METADATA_TIMEOUT_MS * METADATA_TIMEOUT_COUNT_MAX);

    /** Metadata version. used to detect incompatible versions
     * Update on release when something changed. */
    constexpr static int METADATA_VERSION = 1;

    /** Maximum number of clients waiting for shared resource */
    constexpr static int MAX_SIM_CLIENTS = 30;

    class Client_entry
    {
    public:
        uint32_t client_id;
        std::chrono::steady_clock::time_point last_heartbeat_time;
    };

    typedef enum
    {
        /** This must be zero! Because newly created shared memory is zerofilled. */
        DATA_STATE_CREATED = 0,
        /** master_locker was reinitialized, so memory is in unknown state. */
        DATA_STATE_RECOVERED,
        /** memory good to use. */
        DATA_STATE_OK,
    } Data_state;

    typedef struct
    {
        /** metadata version. Must match the METADATA_VERSION in
         * memory once initialized to avoid corruptions.
         * 0 means you are the first client and should initialize the structure.
         */
        uint32_t version;

        uint32_t semaphore_id;

        /** How many clients pending or acquired
         */
        uint32_t client_count;

        /** Unique client ID > 0. Incemented on each instance creation
         */
        uint32_t next_client_id;

        /** client ID which has the metadata locked.
         */
        uint32_t current_client_id;

        /** zero when master_lock has been reset. User must call Release(true) to reset this bit.
         */
        uint32_t data_state;

        /** Waiter list */
        Client_entry clients[MAX_SIM_CLIENTS];
    } Shared_metadata;

    void
    Main_loop();

    Shared_semaphore::Lock_result
    Data_gate_wait();

    void
    Data_gate_open();

    bool
    Insert_client_in_list();

    void
    Remove_client_from_list(uint32_t idx);

    Acquire_result
    Master_lock();

    void
    Initialize_metadata();

    void
    Master_release();

    void
    Dump_client_list(int);

    std::string name;
    uint32_t master_lock_id;
    size_t size;
    size_t align;

    uint32_t my_client_id;

    /**
     * Protects event_*, current_completion_handler,
     * current_request and tread_event
     */
    std::mutex thread_mutex;

    /** Tell mainloop to exit. Used in destructor. */
    bool event_stop = false;
    /** Tell mainloop to initiate acquire process
     * set by Acquire(), reset by mainloop. */
    bool event_acquire = false;
    /** Tell mainloop to Release the shared data
     * set by Release(), reset by mainloop. */
    bool event_release_valid = false;
    /** Tell mainloop to Release the shared data
     * set by Release(), reset by mainloop. */
    bool event_release = false;
    /** Protect from undetermined Release() behavior
     * set by Acquire(), reset by mainloop. */
    bool acquire_in_progress = false;
    std::condition_variable tread_event;
    Acquire_handler current_completion_handler;
    Request::Ptr current_request;

    /** worker thread. */
    std::thread thread;

    Shared_semaphore::Ptr data_gate;
    Shared_semaphore::Ptr master_locker;
    Shared_memory::Ptr shared_memory;
    Shared_metadata* my_metadata;
    void* my_data;
};

} /* namespace vsm */
#endif /* SHARED_DATA_H_ */
