// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle_requests.h
 *
 * Convenience types for all specific vehicle requests.
 */
#ifndef _VEHICLE_REQUESTS_H_
#define _VEHICLE_REQUESTS_H_

#include <ugcs/vsm/vehicle_request.h>
#include <ugcs/vsm/task.h>
#include <ugcs/vsm/vehicle_command.h>

namespace ugcs {
namespace vsm {

/** Vehicle request for ugcs::vsm::Task payload. */
typedef Vehicle_request_spec<Task> Vehicle_task_request;

/** Vehicle request for ugcs::vsm::Vehicle_command payload. */
typedef Vehicle_request_spec<Vehicle_command> Vehicle_command_request;

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VEHICLE_REQUESTS_H_ */
