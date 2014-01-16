// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file move_action.h
 *
 * General navigation action definition.
 */
#ifndef _MOVE_ACTION_H_
#define _MOVE_ACTION_H_

#include <vsm/action.h>
#include <vsm/coordinates.h>
#include <vsm/mavlink.h>

namespace vsm {

/** Move vehicle to the specific position (waypoint). */
class Move_action: public Action {
    DEFINE_COMMON_CLASS(Move_action, Action)

public:

    /** Construct move action explicitly. */
    Move_action(Wgs84_position position, double wait_time, double acceptance_radius,
            double loiter_orbit, double heading, double elevation) :
                Action(Type::MOVE),
                position(position),
                wait_time(wait_time),
                acceptance_radius(acceptance_radius),
                loiter_orbit(loiter_orbit),
                heading(heading),
                elevation(elevation)
            {}

    /**
     * Construct move action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_NAV_WAYPOINT
     */
    Move_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::MOVE),
        position(Geodetic_tuple(item->x * M_PI / 180.0,
                item->y * M_PI / 180.0,
                item->z)),
                wait_time(0.1 * item->param1),
                acceptance_radius(item->param2),
                loiter_orbit(item->param3),
                heading(item->param4 * M_PI / 180.0),
                elevation(item->elevation)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_NAV_WAYPOINT);
    }

    /**
     * Target global position of the movement.
     */
    Wgs84_position position;

    /**
     * Wait interval in seconds.
     * Shows how long the vehicle should stay/hold at the target position.
     */
    double wait_time;

    /**
     * Acceptance radius of the target position in meters.
     * When the sphere with the radius centered at the target position is hit
     * by the vehicle the target position is considered reached.
     */
    double acceptance_radius;

    /**
     * Radius of the point fly-by orbit in meters.
     * Positive value stands for the CW direction, negative - CCW.
     * Use {@code 0} for no-braking pass through the target point.
     */
    double loiter_orbit;

    /**
     * Heading in radians.
     */
    double heading;

    /**
     * Elevation in meters (i.e. terrain height) underneath the position.
     */
    double elevation;
};

/** Type mapper from move action. */
template<>
struct Action::Mapper<Action::Type::MOVE> {
    /** Real type. */
    typedef Move_action type;
};

} /* namespace vsm */

#endif /* _MOVE_ACTION_H_ */
