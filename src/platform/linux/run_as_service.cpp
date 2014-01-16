// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file windows_service.cpp
 */

#include <vsm/run_as_service.h>

using namespace vsm;

/**
 * Nothing here for linux.
 */
bool
vsm::Run_as_service(const char*, int, char *[], Program_init, Program_done)
{
    return false;
}
