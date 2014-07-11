// Copyright (c) 2014, Smart Projects Holdings Ltd
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
    Change_speed_action(double speed) :
    Action(Type::CHANGE_SPEED),
    speed(speed) {}

    /**
     * Construct change speed action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_DO_CHANGE_SPEED
     */
    Change_speed_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::CHANGE_SPEED),
        speed(item->param2)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_DO_CHANGE_SPEED);
    }


    /**
     * Target airspeed in meters per second.
     */
    double speed;
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
