// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file change_speed_action.h
 *
 * Change airspeed action.
 */
#ifndef _CHANGE_SPEED_ACTION_H_
#define _CHANGE_SPEED_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/coordinates.h>

namespace ugcs {
namespace vsm {

/** Immediate change or vehicle airspeed. */
class Change_speed_action: public Action {
    DEFINE_COMMON_CLASS(Change_speed_action, Action)

public:
    /** Construct action explicitly. */
    Change_speed_action(float h_speed, float v_speed) :
    Action(Type::CHANGE_SPEED),
    speed(h_speed),
    vertical_speed(v_speed)
    {}

    /**
     * Construct change speed action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_DO_CHANGE_SPEED_EX
     */
    Change_speed_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::CHANGE_SPEED),
        speed(item->param1),
        vertical_speed(item->param2)
     {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CHANGE_SPEED_EX);
     }

    /**
     * Construct action from protobuf command.
     */
    Change_speed_action(const Property_list& p) :
        Action(Type::CHANGE_SPEED)
    {
        p.at("ground_speed")->Get_value(speed);
        p.at("vertical_speed")->Get_value(vertical_speed);
    }

    /**
     * Target ground speed in meters per second.
     */
    double speed;
    /**
     * Target vertical speed in meters per second.
     */
    float vertical_speed;
};

/** Type mapper for change speed action. */
template<>
struct Action::Mapper<Action::Type::CHANGE_SPEED> {
    /** Real type. */
    typedef Change_speed_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _CHANGE_SPEED_ACTION_H_ */
