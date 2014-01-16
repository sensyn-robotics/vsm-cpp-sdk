// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_memory.cpp
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#include <vsm/shared_memory.h>

namespace // anonymous
{

class Shared_memory_win: public vsm::Shared_memory
{
    DEFINE_COMMON_CLASS(Shared_memory_win, Shared_memory)
public:
    Shared_memory_win();
    virtual
    ~Shared_memory_win();

    virtual Open_result
    Open(const std::string& name, const size_t);

    virtual void
    Close();

private:
    HANDLE file;
};

Shared_memory_win::Shared_memory_win()
:file(NULL)
{
}

Shared_memory_win::~Shared_memory_win()
{
    Close();
}

vsm::Shared_memory::Open_result
Shared_memory_win::Open(const std::string& name, const size_t size)
{
    if (size == 0) {
        return Shared_memory::OPEN_RESULT_ERROR;
    }
    auto ret = Shared_memory::OPEN_RESULT_OK;
    /** Creates file mapping in "Local" session context
     * Cannot do global context because that requires SeCreateGlobalPrivilege
     */
    file = CreateFileMapping(
                  INVALID_HANDLE_VALUE,     // use paging file
                  NULL,                     // default security
                  PAGE_READWRITE,           // read/write access
                  0,                        // maximum object size (high-order DWORD)
                  size,                     // maximum object size (low-order DWORD)
                  name.c_str());                 // name of mapping object
    if (file == NULL) {
        LOG_ERR("CreateFileMapping failed");
        return Shared_memory::OPEN_RESULT_ERROR;
    } else {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
//            LOG("Opened existing memory %s ", name.c_str());
        } else {
//            LOG("Created new memory %s", name.c_str());
            ret = Shared_memory::OPEN_RESULT_CREATED;
        }
    }

    memory = MapViewOfFile(
                file,                   // handle to map object
                FILE_MAP_ALL_ACCESS,    // read/write permission
                0,
                0,
                size);
    if (memory == NULL) {
        CloseHandle(file);
        LOG_ERR("MapViewOfFile failed");
        return Shared_memory::OPEN_RESULT_ERROR;
    }
    return ret;
}

void
Shared_memory_win::Close()
{
    if (memory) {
        UnmapViewOfFile(memory);
        CloseHandle(file);
        memory = nullptr;
    }
}

}// anonymous namespace

namespace vsm
{

bool
Shared_memory::Delete(const std::string&)
{
    LOG_ERR("Shared_memory::Delete not supported");
    return false;
}

Shared_memory::Ptr
Shared_memory::Create()
{
    return Shared_memory_win::Create();
}

} /* namespace vsm */
