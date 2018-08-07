// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Platform-dependent part for File_processor and nested classes.
 * Linux-specific part.
 */

#include <ugcs/vsm/posix_file_handle.h>
#include <ugcs/vsm/debug.h>
#include <cstring>

using namespace ugcs::vsm;

File_processor::Stream::Native_handle::Unique_ptr
File_processor::Open_native_handle(
        const std::string &name,
        const std::string &mode)
{
    return std::make_unique<internal::Posix_file_handle>(name, mode);
}

std::unique_ptr<File_processor::Native_controller>
File_processor::Native_controller::Create()
{
    return std::make_unique<internal::Poll_io_controller>();
}

FILE*
File_processor::Fopen_utf8(const std::string &name, const std::string & mode)
{
    return std::fopen(name.c_str(), mode.c_str());
}

bool
File_processor::Rename_utf8(const std::string &old_name, const std::string &new_name)
{
    return std::rename(old_name.c_str(), new_name.c_str()) == 0;
}

bool
File_processor::Remove_utf8(const std::string &name)
{
    return std::remove(name.c_str()) == 0;
}

int
File_processor::Access_utf8(const std::string &name, int mode)
{
    return access(name.c_str(), mode);
}

