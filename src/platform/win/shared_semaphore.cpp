// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_memory.cpp
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#include <ugcs/vsm/shared_semaphore.h>
#include <ugcs/vsm/windows.h>

namespace
{

class Shared_semaphore_win: public ugcs::vsm::Shared_semaphore
{
    DEFINE_COMMON_CLASS(Shared_semaphore_win, Shared_semaphore)
public:
    Shared_semaphore_win();
    virtual
    ~Shared_semaphore_win();

    virtual Open_result
    Open(const std::string& name, int initial_count, int max_count);

    virtual Lock_result
    Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    virtual bool
    Try_wait();

    virtual void
    Signal();

    virtual void
    Close();

private:
    HANDLE semaphore;
};

Shared_semaphore_win::Shared_semaphore_win()
:semaphore(NULL)
{
}

Shared_semaphore_win::~Shared_semaphore_win()
{
    Close();
}

ugcs::vsm::Shared_semaphore::Open_result
Shared_semaphore_win::Open(const std::string& name, int initial_count, int max_count)
{
    Close();
//    LOG("Creating semaphore %s ", name.c_str());
    semaphore = CreateSemaphore(nullptr, initial_count, max_count, name.c_str());

    if (semaphore == NULL) {
        return Shared_semaphore::OPEN_RESULT_ERROR;
    } else {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
//            LOG("Opened existing semaphore %s ", name.c_str());
            return Shared_semaphore::OPEN_RESULT_OK;
        } else {
//            LOG("Created new semaphore %s", name.c_str());
            return Shared_semaphore::OPEN_RESULT_CREATED;
        }
    }
}

void
Shared_semaphore_win::Close()
{
    if (semaphore) {
        CloseHandle(semaphore);
        semaphore = NULL;
    }
}

ugcs::vsm::Shared_semaphore::Lock_result
Shared_semaphore_win::Wait(std::chrono::milliseconds timeout)
{

    DWORD t = timeout.count();
    if (timeout == std::chrono::milliseconds::max()) {
        t = INFINITE;
    }
    switch (WaitForSingleObject(semaphore, t))
    {
    case WAIT_OBJECT_0:
        return WAIT_RESULT_OK;
    case WAIT_TIMEOUT:
        return WAIT_RESULT_TIMEOUT;
    default:
        break;
    }
    return WAIT_RESULT_ERROR;
}

bool
Shared_semaphore_win::Try_wait()
{
    switch (WaitForSingleObject(semaphore, 0))
    {
    case WAIT_OBJECT_0:
        return true;
    case WAIT_TIMEOUT:
        return false;
    default:
        break;
    }
    VSM_SYS_EXCEPTION("Failed to acquire semaphore!");
    return false;
}

void
Shared_semaphore_win::Signal()
{
    if (!ReleaseSemaphore(semaphore, 1, nullptr)) {
        LOG("ReleaseSemaphore error: %s", ugcs::vsm::Log::Get_system_error().c_str());
    }
}

} /* namespace anonymous */

namespace ugcs {
namespace vsm {

Shared_semaphore::Ptr
Shared_semaphore::Create()
{
    return Shared_semaphore_win::Create();
}

} /* namespace vsm */
} /* namespace ugcs */
