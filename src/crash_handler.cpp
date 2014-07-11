// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/crash_handler.h>
#include <ugcs/vsm/log.h>

using namespace ugcs::vsm;

std::string Crash_handler::reports_file_base;

void
Crash_handler::Set_reports_file_base(const std::string& base)
{
    reports_file_base = base;
    LOG_DBG("Crash handler reports file base is set to [%s]", base.c_str());
    Enable();
}
