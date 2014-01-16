// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file takeoff_action.h
 */
#ifndef _TAKEOFF_ACTION_H_
#define _TAKEOFF_ACTION_H_

#include <vsm/action.h>

namespace vsm {

/** Take off from the specified position and reach specified altitude. */
class Takeoff_action: public Action {
    DEFINE_COMMON_CLASS(Takeoff_action, Action)

public:

    /** Construct take-off action explicitly. */
    Takeoff_action(Wgs84_position position, double heading, double elevation,
            double climb_rate, double acceptance_radius) :
    Action(Type::TAKEOFF),
    position(position),
    heading(heading),
    elevation(elevation),
    climb_rate(climb_rate),
    acceptance_radius(acceptance_radius) {}

    /**
     * Construct take-off action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_TAKEOFF_EX
     */
    Takeoff_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::TAKEOFF),
        position(Geodetic_tuple(item->x * M_PI / 180.0,
                                item->y * M_PI / 180.0,
                                item->z)),
        heading(item->param4 * M_PI / 180.0),
        elevation(item->elevation),
        climb_rate(item->param3),
        acceptance_radius(item->param2)
    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_TAKEOFF_EX);
    }


    /** Take-off position. After take-off, the vehicle should climb to the
     * altitude defined in the position. */
    Wgs84_position position;

    /** Heading in radians. */
    double heading;

    /** Terrain height in meters underneath the position. */
    double elevation;

    /** Climbing rate, in meters per second. */
    double climb_rate;

    /** Acceptance radius of the position. Maximum distance at which the position
     * is considered reached by the vehicle. Set in meters. */
    double acceptance_radius;
};

/** Mapping for take-off action. */
template<>
struct Action::Mapper<Action::Type::TAKEOFF> {
    /** Real type. */
    typedef Takeoff_action type;
};

} /* namespace vsm */

#endif /* _TAKEOFF_ACTION_H_ */
