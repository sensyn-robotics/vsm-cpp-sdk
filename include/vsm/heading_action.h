// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file heading_action.h
 */
#ifndef _HEADING_ACTION_H_
#define _HEADING_ACTION_H_

#include <vsm/action.h>
#include <vsm/mavlink.h>

namespace vsm {

/** Change the heading of a vehicle. Heading is an angle between the North and
 * head (nose) of the vehicle. If vehicle can't change the heading without
 * changing the movement vector, then payload heading should be changed instead.
 */
class Heading_action: public Action {
    DEFINE_COMMON_CLASS(Heading_action, Action)

public:

    /** Construct the heading action explicitly. */
    Heading_action(double heading) :
        Action(Type::HEADING),
        heading(heading) {}


    /**
     * Construct heading action from Mavlink mission item.
     * @throw Action::Format_exception if mission item has wrong format.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_CONDITION_YAW
     */
    Heading_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::HEADING),
        heading(item->param1 * M_PI / 180.0)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_CONDITION_YAW);
        if (item->param4) {
            VSM_EXCEPTION(Action::Format_exception,
                    "Heading action wrong angle type %f", item->param4.Get());
        }
    }

    /**
     * Heading in radians.
     */
    double heading;
};

/** Type mapper for Heading action. */
template<>
struct Action::Mapper<Action::Type::HEADING> {
    /** Real type. */
    typedef Heading_action type;
};

}; /* namespace vsm */

#endif /* _HEADING_ACTION_H_ */
