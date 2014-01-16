// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_memory.cpp
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

#include <vsm/shared_memory.h>

namespace // anonymous
{

class Shared_memory_linux: public vsm::Shared_memory
{
    DEFINE_COMMON_CLASS(Shared_memory_linux, Shared_memory)
public:
    Shared_memory_linux();
    virtual
    ~Shared_memory_linux();

    virtual Open_result
    Open(const std::string& name, const size_t size);

    virtual void
    Close();

private:

    int file;
    size_t size;
    std::string native_name;
};

Shared_memory_linux::Shared_memory_linux():
file(-1),size(0)
{
}

Shared_memory_linux::~Shared_memory_linux()
{
    Close();
}

vsm::Shared_memory::Open_result
Shared_memory_linux::Open(const std::string& name, const size_t size)
{
    Close();
    if (size == 0) {
        return Shared_memory::OPEN_RESULT_ERROR;
    }

    auto ret = OPEN_RESULT_CREATED;
    this->size = size;
    native_name = std::string("/" + name);
    file = shm_open(native_name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (file == -1) {
        if (errno == EEXIST) {
            file = shm_open(native_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
            ret = OPEN_RESULT_OK;
        }
    }
    if (file == -1) {
        ret = OPEN_RESULT_ERROR;
    } else {
        if (ftruncate(file, size) == -1) {
            VSM_SYS_EXCEPTION("ftruncate failed");
        }
        memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
        if (memory == nullptr) {
            close(file);
            VSM_SYS_EXCEPTION("mmap failed");
            ret = OPEN_RESULT_ERROR;
        }
    }
    return ret;
}

void
Shared_memory_linux::Close()
{
    if (memory) {
        munmap(memory, size);
        close(file);
        memory = nullptr;
        file = -1;
    }
}
}   // anonymous namespace

namespace vsm
{

bool
Shared_memory::Delete(const std::string& name)
{
    if (shm_unlink(name.c_str()) != 0) {
        LOG("shm_unlink error: %s", Log::Get_system_error().c_str());
        return false;
    }
    return true;
}

Shared_memory::Ptr
Shared_memory::Create()
{
    return Shared_memory_linux::Create();
}

} /* namespace vsm */
