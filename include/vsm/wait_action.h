// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file wait_action.h
 */
#ifndef _WAIT_ACTION_H_
#define _WAIT_ACTION_H_

#include <vsm/action.h>
#include <vsm/mavlink.h>
#include <chrono>

namespace vsm {

/** Wait action. Hold at the current position for specified amount of time.
 * Implementation depends on vehicle type. */
class Wait_action: public Action {
    DEFINE_COMMON_CLASS(Wait_action, Action)

public:
    /** Construct wait action explicitly. */
    Wait_action(double wait_time) :
        Action(Type::WAIT),
        wait_time(wait_time) {}

    /**
     * Construct wait action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_CONDITION_DELAY
     */
    Wait_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::WAIT),
        wait_time(0.1 * item->param1)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_CONDITION_DELAY);
    }

    /** Time to wait in seconds. */
    double wait_time;
};

/** Type mapper for wait action. */
template<>
struct Action::Mapper<Action::Type::WAIT> {
    /** Real type. */
    typedef Wait_action type;
};

} /* namespace vsm */

#endif /* _WAIT_ACTION_H_*/
