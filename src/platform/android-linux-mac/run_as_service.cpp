// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file windows_service.cpp
 */

#include <ugcs/vsm/run_as_service.h>

using namespace ugcs::vsm;

/**
 * Nothing here for linux.
 */
Run_as_service_result
ugcs::vsm::Run_as_service(const char*, int, char *[], Program_init, Program_done)
{
    return SERVICE_RESULT_NORMAL_INVOCATION;
}
