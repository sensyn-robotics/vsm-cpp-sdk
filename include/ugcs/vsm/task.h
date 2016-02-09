// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file task.h
 *
 * Vehicle task payload definition.
 */

#ifndef _TASK_H_
#define _TASK_H_

#include <vector>
#include <ugcs/vsm/action.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/task_attributes_action.h>
#include <ugcs/vsm/optional.h>

namespace ugcs {
namespace vsm {

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

    /** Retrieve home position from action with the altitude relative to the
     * ground level. Either get SET_HOME or the first MOVE action.
     */
    Wgs84_position
    Get_home_position_relative() const;

    /** Get terrain height at home position in meters. */
    double
    Get_home_position_elevation() const;

    /** Get take-off altitude, that is the absolute altitude of the position where
     * the vehicle was or will be launched.
     */
    double
    Get_takeoff_altitude() const;

    /** Internal SDK method. Used to set the take-off altitude. VSM user is not
     * supposed to use it. */
    void
    Set_takeoff_altitude(double altitude);

    /** Action list of the task .*/
    std::vector<Action::Ptr> actions;

    /** Task attributes action. If nullptr, then vehicle defaults should
     * be used.
     */
    Task_attributes_action::Ptr attributes;

    /** Parameter list for the task .*/
    std::vector<Action::Ptr> parameters;

private:

    /** Implementation of retrieve home position from the task. Either gets
     * SET_HOME or the first MOVE action.
     * @return <position, elevation> tuple;
     */
    std::tuple<Wgs84_position, double>
    Get_home_position_impl() const;

    /** Take-off altitude, should be set before giving the task for user. */
    Optional<double> takeoff_altitude;

};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VSM_MISSION_H_ */
