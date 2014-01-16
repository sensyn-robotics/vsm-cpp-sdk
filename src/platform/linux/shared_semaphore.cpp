// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_semaphore.cpp
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#include <chrono>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <vsm/shared_semaphore.h>

namespace
{
class Shared_semaphore_linux: public vsm::Shared_semaphore
{
    DEFINE_COMMON_CLASS(Shared_semaphore_linux, Shared_semaphore)
public:
    Shared_semaphore_linux();
    virtual
    ~Shared_semaphore_linux();

    virtual Open_result
    Open(const std::string& name, int initial_count, int max_count);

    virtual void
    Close();

    virtual Lock_result
    Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    virtual bool
    Try_wait();

    virtual void
    Signal();

private:
    int
    Get_count();

    std::string native_name;
    sem_t* semaphore;
    int max_count;
};


Shared_semaphore_linux::Shared_semaphore_linux()
:semaphore(SEM_FAILED),max_count(0)
{
}

Shared_semaphore_linux::~Shared_semaphore_linux()
{
    Close();
}

vsm::Shared_semaphore::Open_result
Shared_semaphore_linux::Open(const std::string& name, int initial_count, int max_count)
{
    Close();
    native_name = std::string("/" + name);
    auto ret = OPEN_RESULT_CREATED;
    this->max_count = max_count;
    semaphore = sem_open(native_name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, initial_count);
    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            semaphore = sem_open(native_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR, initial_count);
            ret = OPEN_RESULT_OK;
        }
    }
    if (semaphore == SEM_FAILED) {
        ret = OPEN_RESULT_ERROR;
    } else {
        int val = -1;
        sem_getvalue(semaphore, &val);
//        LOG("New semaphore %s count=%d, created=%d", native_name.c_str(), val, ret);
    }
    return ret;
}

void
Shared_semaphore_linux::Close()
{
    if (semaphore != SEM_FAILED) {
        if (sem_close(semaphore) != 0)
        semaphore = SEM_FAILED;
    }
}

vsm::Shared_semaphore::Lock_result
Shared_semaphore_linux::Wait(std::chrono::milliseconds timeout)
{
    if (semaphore == SEM_FAILED) {
        return WAIT_RESULT_ERROR;
    }
    struct timespec t_realtime;
    if (timeout != std::chrono::milliseconds::max()) {
        // Convert given timeout to absolute time point for sem_wait()
        if (clock_gettime(CLOCK_REALTIME, &t_realtime) == -1) {
            VSM_SYS_EXCEPTION("Failed fetch realtime clock!");
        }
        int nanosecs = timeout.count();
        int seconds = nanosecs / 1000;
        nanosecs %= 1000;
        nanosecs *= 1000000;
        t_realtime.tv_nsec += nanosecs;
        if (t_realtime.tv_nsec > 1000000000) {
            t_realtime.tv_nsec -= 1000000000;
            t_realtime.tv_sec++;
        }
        t_realtime.tv_sec += seconds;
    }
    while (true) {
	    int err;
		if (timeout == std::chrono::milliseconds::max()) {
			err = sem_wait(semaphore);
		} else {
			err = sem_timedwait(semaphore, &t_realtime);
            //TODO Calculate monotonic time distance and if that differs
            // significantly from specified timeout, signal error.
            // ie. that means that somebody has updated the RTC while we were waiting.
            // This is a bug in kernel which does not allow set semaphore
            // timeout based on monotonic clock.
		}
		if (err == 0) {
            // poor man's workaround for max_count enforcement.
            // Repeat wait until we are back within max_count.
            if (Get_count() < max_count) {
                return WAIT_RESULT_OK;
            }
            LOG("repeat wait");
		} else {
		    switch (errno) {
            case ETIMEDOUT:
                return WAIT_RESULT_TIMEOUT;
            case EINTR:
                break;  // wait again
            default:
                VSM_SYS_EXCEPTION("Error sem_wait!");
                return WAIT_RESULT_ERROR;
            }
		}
	}
}

bool
Shared_semaphore_linux::Try_wait()
{
    while (true) {
        if (sem_trywait(semaphore) == 0) {
            // poor man's workaround for max_count enforcement.
            // Repeat wait until we are back within max_count.
            if (Get_count() < max_count)
                return true;
        } else {
            switch (errno) {
            case EAGAIN:
                return false;
            case EINTR:
                break;  // wait again
            default:
                return false;
            }
        }
    }
    return false;
}

void
Shared_semaphore_linux::Signal()
{
    // poor man's workaround for max_count enforcement.
    // signal only if current count is less than allowed.
    if (Get_count() < max_count) {
        if (sem_post(semaphore) != 0) {
            VSM_SYS_EXCEPTION("Error releasing semaphore!");
        }
    } else {
        LOG("Semaphore value exceeds specified max (%d)", max_count);
    }
}

int
Shared_semaphore_linux::Get_count()
{
    int val;
    if (sem_getvalue(semaphore, &val) !=0 ) {
        VSM_SYS_EXCEPTION("sem_getvalue error");
    }
    return val;
}
}

namespace vsm
{

bool
Shared_semaphore::Delete(const std::string& name)
{
    if (sem_unlink(name.c_str()) != 0) {
        return false;
    }
    return true;
}

Shared_semaphore::Ptr
Shared_semaphore::Create()
{
    return Shared_semaphore_linux::Create();
}

} /* namespace vsm */
