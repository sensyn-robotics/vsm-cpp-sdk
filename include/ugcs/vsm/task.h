// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file task.h
 *
 * Vehicle task payload definition.
 */

#ifndef _UGCS_VSM_TASK_H_
#define _UGCS_VSM_TASK_H_

#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/task_attributes_action.h>
#include <ugcs/vsm/optional.h>
#include <ugcs/vsm/action.h>
#include <vector>

namespace ugcs {
namespace vsm {

typedef std::shared_ptr<ugcs::vsm::proto::Vsm_message> Proto_msg_ptr;

/** Action plan for a single vehicle. */
class Task {
public:
    /** Constructor.
     * @param reserved_size Initial size of the actions vector.
     */
    Task(size_t reserved_size = 0)
    {
        if (reserved_size) {
            actions.reserve(reserved_size);
        }
    }

    /** Retrieve home position from action. Either get SET_HOME or the first
     * MOVE action.
     */
    Wgs84_position
    Get_home_position() const;

    /** Get terrain height at home position in meters. */
    double
    Get_home_position_altitude() const;

    /** Get take-off altitude, that is the absolute altitude of the position where
     * the vehicle was or will be launched.
     */
    double
    Get_takeoff_altitude() const;

    /** Internal SDK method. Used to set the take-off altitude. VSM user is not
     * supposed to use it. */
    void
    Set_takeoff_altitude(double altitude);

    double
    Get_takeoff_altitude_above_ground() const;
    void
    Set_takeoff_altitude_above_ground(double altitude);

    /** Action list of the task .*/
    std::vector<Action::Ptr> actions;

    /** Task attributes action. If nullptr, then vehicle defaults should
     * be used.
     */
    Task_attributes_action::Ptr attributes;

    /** Parameter list for the task .*/
    Property_list parameters;

    Proto_msg_ptr ucs_response;

    bool return_native_route = false;
    bool use_crlf_in_native_route = false;

private:
    /** Implementation of retrieve home position from the task. Either gets
     * SET_HOME or the first MOVE action.
     * @return <position, elevation> tuple;
     */
    std::tuple<Wgs84_position, double>
    Get_home_position_impl() const;

    /** Take-off altitude above mean sea level, should be set before giving the task for user. */
    Optional<double> takeoff_altitude;

    // Take-off point altitude above ground
    // Sensyn specification
    // Sensyn pilot will set 500m Take-off point altitude above ground into altitude_amsl for fly over some low altitude
    // Not really set 500m altitude for fly height
    double takeoff_altitude_above_ground = 0;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_TASK_H_ */
