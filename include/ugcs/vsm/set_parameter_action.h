// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file heading_action.h
 */
#ifndef _SET_PARAMETER_ACTION_H_
#define _SET_PARAMETER_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Change the heading of a vehicle. Heading is an angle between the North and
 * head (nose) of the vehicle. If vehicle can't change the heading without
 * changing the movement vector, then payload heading should be changed instead.
 */
class Set_parameter_action: public Action {
    DEFINE_COMMON_CLASS(Set_parameter_action, Action)

public:

    /**
     * Construct heading action from Mavlink mission item.
     * @throw Action::Format_exception if mission item has wrong format.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_CONDITION_YAW
     */
    Set_parameter_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::SET_PARAMETER),
        param_type(static_cast<mavlink::ugcs::MAV_MISSION_PARAMETER_TYPE>(item->param1.Get())),
        param_value(item->param2)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_DO_SET_PARAMETER);
    }

    mavlink::ugcs::MAV_MISSION_PARAMETER_TYPE param_type;
    /**
     * Value
     */
    float param_value;
};

/** Type mapper for Heading action. */
template<>
struct Action::Mapper<Action::Type::SET_PARAMETER> {
    /** Real type. */
    typedef Set_parameter_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _SET_PARAMETER_ACTION_H_ */
