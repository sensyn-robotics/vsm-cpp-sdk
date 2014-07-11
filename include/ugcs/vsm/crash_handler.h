// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file crash_handler.h
 */
#ifndef _CRASH_HANDLER_H_
#define _CRASH_HANDLER_H_

#include <string>

namespace ugcs {
namespace vsm {

/** Crash handler collects some useful information in case of program failure
 * and stores it on disk.
 */
class Crash_handler {

public:

    /** Set file name base for reports. Enabled crash handler when set. */
    static void
    Set_reports_file_base(const std::string&);

private:

    /** Called to enable crash handler. To be implemented by platform specific
     * code.
     */
    static void
    Enable();

    /** File name for the crash reports. */
    static std::string reports_file_base;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _CRASH_HANDLER_H_ */
