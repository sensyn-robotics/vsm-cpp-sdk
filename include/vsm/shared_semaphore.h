// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_semaphore.h
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#ifndef SHARED_SEMAPHORE_H_
#define SHARED_SEMAPHORE_H_

#include <chrono>
#include <vsm/utils.h>

namespace vsm
{

/** Platform independent implementation of system-wide named semaphore
 * used for interprocess communications.
 *
 * User must be aware of the following platform specific differences
 * in shared semaphore semantics:
 * Windows:
 *   1) When the last handle to semaphore is closed the OS destroys the semaphore.
 *      (Note: OS closes all open handles on application termination/crash)
 *   2) By default the semaphore is created in Session Kernel object namespace.
 *   3) Semaphore cannot be explicitly deleted.
 *   Implemented via CreateSemaphore() and friends.
 *
 * Linux:
 *   1) Semaphore persists in the system until explicitly deleted or system restarted.
 *   2) It is accessible from any session of its creator user.
 *   3) Semaphore max count is not supported on Linux.
 *   Implemented via sem_open() and friends.
 *
 */
class Shared_semaphore: public std::enable_shared_from_this<Shared_semaphore>
{
    DEFINE_COMMON_CLASS(Shared_semaphore, Shared_semaphore)
public:

    /** Possible return codes from Wait() call */
    typedef enum {
        WAIT_RESULT_OK,
        WAIT_RESULT_ERROR,
        WAIT_RESULT_TIMEOUT
    } Lock_result;

    /** Possible return codes from Open() call */
    typedef enum {
        OPEN_RESULT_OK,
        OPEN_RESULT_CREATED,
        OPEN_RESULT_ERROR
    } Open_result;

    /**
     * Constructor should not be called explicitly.
     * Use Create() function to instantiate the object.
     */
    Shared_semaphore(){};

    virtual
    ~Shared_semaphore(){};

    /**
     * Opens/creates a system-wide named semaphore.
     *  Closes previously opened semaphore if any.
     * @param name          Semaphore name
     * @param initial_count Initial count. Ignored if semaphore already exists.
     * @param max_count     Max count. Ignored if semaphore already exists.
     * @return OPEN_RESULT_OK Existing semaphore opened successfully.
     *         OPEN_RESULT_CREATED Semaphore Created successfully.
     *         OPEN_RESULT_ERROR Error while creating semaphore.
     *                           Previously opened semaphore is closed.
     */
    virtual Open_result
    Open(const std::string& name, int initial_count, int max_count) = 0;

    /**
     * Closes previously opened semaphore.
     */
    virtual void
    Close() = 0;

    /**
     * Wait for semaphore.
     * @param timeout Timeout. If not specified, waits infinitely.
     * @return WAIT_RESULT_OK Semaphore acquired.
     *         WAIT_RESULT_TIMEOUT Semaphore wait timeouted.
     *         WAIT_RESULT_ERROR Error while acquiring semaphore.
     */
    virtual Lock_result
    Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * Try to acquire semaphore.
     * @return true Semaphore acquired.
     *         false Semaphore already locked or error.
     */
    virtual bool
    Try_wait() = 0;

    /**
     * Release semaphore.
     *   (On windows increments semaphore count by 1)
     */
    virtual void
    Signal() = 0;

    /**
     * Creates Platform specific class instance.
     * This does not create the semaphore itself,
     * you should call Open() to create the named semaphore.
     * @return shared_ptr<Shared_semaphore> of newly created instance
     */
    static Ptr
    Create();

    /**
     * Deletes the named semaphore. (Linux-only)
     *  Does not affect any opened semaphores with this name.
     * @param name Semaphore name.
     * @return true Semaphore deleted.
     *         false error occurred.
     */
    static bool
    Delete(const std::string& name);
};
} /* namespace vsm */
#endif /* SHARED_SEMAPHORE_H_ */
