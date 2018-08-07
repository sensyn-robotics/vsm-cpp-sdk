// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_memory.h
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#ifndef _UGCS_VSM_SHARED_MEMORY_H_
#define _UGCS_VSM_SHARED_MEMORY_H_

#include <ugcs/vsm/utils.h>

namespace ugcs {
namespace vsm {

/** Platform independent implementation of system-wide named shared memory
 * used for interprocess communications.
 * Workflow:
 *  1) Call Shared_memory::Create() to create class instance.
 *  2) Call Shared_memory::Open() to open or create shared memory.
 *  3) Call Shared_memory::Get() to Get the pointer to shared memory.
 *
 *  The pointer is valid until object is closed.
 *  Object is closed:
 *    - explicitly via call to Close()
 *    - implicitly when calling Open() on the same instance.
 *    - in destructor.
 *
 * User must be aware of the following platform specific differences
 * in shared memory semantics:
 * Windows:
 *   1) When the last handle to memory is closed the OS destroys the memory.
 *      (Note: OS closes all open handles on application termination/crash)
 *   2) By default the memory is created in Session Kernel object namespace.
 *   3) Shared memory cannot be explicitly deleted.
 *   Implemented via CreateFileMapping() and friends.
 *
 * Linux:
 *   1) Shared memory persists in the system until explicitly deleted or system restarted.
 *   2) It is accessible from any session of its creator user.
 *   Implemented via shm_open() and friends.
 *
 */
class Shared_memory: public std::enable_shared_from_this<Shared_memory>
{
    DEFINE_COMMON_CLASS(Shared_memory, Shared_memory)
public:
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
    Shared_memory():
    memory(nullptr)
    {};

    virtual
    ~Shared_memory() {}

    /**
     * Open/create shared memory.
     *  Closes previously opened memory if any.
     * @param name Name of memory object
     * @param size size in bytes. Will return error if size==0
     * @return OPEN_RESULT_OK Opened existing shared memory.
     *         OPEN_RESULT_CREATED Created new shared memory.
     *         OPEN_RESULT_ERROR Error while creating shared memory.
     *                           Previously opened memory is closed.
     */
    virtual Open_result
    Open(const std::string& name, const size_t size) = 0;

    /**
     * Closes previously opened memory.
     */
    virtual void
    Close() = 0;

    /**
     * Returns the pointer to shared memory.
     * @return pointer to shared memory. Returns nullptr if not opened.
     */
    virtual void*
    Get() {return memory;}

    /**
     * Deletes the named memory. (Linux-only)
     *  Does not affect any opened memory with this name.
     * @param name Shared memory name.
     * @return true Shared memory deleted.
     *         false error occurred.
     */
    static bool
    Delete(const std::string& name);

    /**
     * Creates Platform specific class instance.
     * This does not create the memory itself,
     * you should call Open() to create the named memory.
     * @return shared_ptr<Shared_memory> of newly created instance
     */
    static Ptr
    Create();

    /** pointer to shared memory */
protected:
    void* memory;
};

} /* namespace vsm */
} /* namespace ugcs */
#endif /* _UGCS_VSM_SHARED_MEMORY_H_ */
