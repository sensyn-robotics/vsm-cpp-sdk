// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Platform-dependent part for File_processor and nested classes.
 * Windows-specific part.
 */

#include <ugcs/vsm/overlapped_io_controller.h>
#include <ugcs/vsm/windows_wstring.h>

using namespace ugcs::vsm;

File_processor::Stream::Native_handle::Unique_ptr
File_processor::Open_native_handle(
        const std::string &name,
        const std::string &mode)
{
    return std::make_unique<internal::Windows_file_handle>(name, mode);
}

std::unique_ptr<File_processor::Native_controller>
File_processor::Native_controller::Create()
{
    return std::make_unique<internal::Overlapped_io_controller>();
}

FILE*
File_processor::Fopen_utf8(const std::string &name, const std::string & mode)
{
    try {
        return _wfopen(Windows_wstring(name), Windows_wstring(mode));
    } catch (const Windows_wstring::Conversion_failure&) {
        return nullptr;
    }

}

bool
File_processor::Rename_utf8(const std::string &old_name, const std::string &new_name)
{
    try {
        return _wrename(Windows_wstring(old_name), Windows_wstring(new_name)) == 0;
    } catch (const Windows_wstring::Conversion_failure&) {
        return false;
    }
}

bool
File_processor::Remove_utf8(const std::string &name)
{
    try {
        return _wremove(Windows_wstring(name)) == 0;
    } catch (const Windows_wstring::Conversion_failure&) {
        return false;
    }
}

int
File_processor::Access_utf8(const std::string &name, int mode)
{
    try {
        return _waccess(Windows_wstring(name), mode);
    } catch (const Windows_wstring::Conversion_failure&) {
        return EINVAL;
    }
}
