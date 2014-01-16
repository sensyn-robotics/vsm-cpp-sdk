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

#include <vsm/vehicle_request.h>
#include <vsm/task.h>
#include <vsm/clear_all_missions.h>
#include <vsm/vehicle_command.h>

namespace vsm {

/** Vehicle request for vsm::Task payload. */
typedef Vehicle_request_spec<Task> Vehicle_task_request;

/** Vehicle request for vsm::Clear_all_missions payload. */
typedef Vehicle_request_spec<Clear_all_missions> Vehicle_clear_all_missions_request;

/** Vehicle request for vsm::Vehicle_command payload. */
typedef Vehicle_request_spec<Vehicle_command> Vehicle_command_request;

} /* namespace vsm */

#endif /* _VEHICLE_REQUESTS_H_ */
