// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _UGCS_VSM_WINDOWS_WSTRING_H_
#define _UGCS_VSM_WINDOWS_WSTRING_H_

#include <string>
#include <windows.h>
#include <ugcs/vsm/exception.h>

namespace ugcs {
namespace vsm {

/** Helper class for constructing Windows API compatible wchar strings from UTF-8 strings. */
class Windows_wstring {
public:
    /** Indicate a problem with data encoding/decoding/conversion. */
    VSM_DEFINE_EXCEPTION(Conversion_failure);

    /** Maximum size of Windows wide char string. */
    static constexpr size_t MAX_WLEN = 8 * 1024;

    /** Construct from UTF-8 string.
     * @throw Conversion_failure in case of conversion error. */
    Windows_wstring(const std::string&);

    /** Get Windows wide char string. */
    LPCWSTR
    Get() const;

    /** Type cast operator for convenience. */
    operator LPCWSTR() const;

private:

    /** Resulting wide char string. */
    WCHAR wchar_str[MAX_WLEN];

};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_WINDOWS_WSTRING_H_ */
