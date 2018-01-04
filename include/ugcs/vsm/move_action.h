// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file move_action.h
 *
 * General navigation action definition.
 */
#ifndef _MOVE_ACTION_H_
#define _MOVE_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/optional.h>
#include <ugcs/vsm/property.h>

namespace ugcs {
namespace vsm {

/** Move vehicle to the specific position (waypoint). */
class Move_action: public Action {
    DEFINE_COMMON_CLASS(Move_action, Action)

public:
    /**
     * Turn type at waypoint.
     */
    typedef enum {
        TURN_TYPE_STOP_AND_TURN = 0,
        TURN_TYPE_STRAIGHT = 1,
        TURN_TYPE_SPLINE = 2,
        TURN_TYPE_BANK_TURN = 3,
    } Turn_type;

    /** Construct move action explicitly. */
    Move_action(Wgs84_position position, double wait_time, double acceptance_radius,
            double loiter_orbit, double heading, double elevation) :
                Action(Type::MOVE),
                position(position),
                wait_time(wait_time),
                acceptance_radius(acceptance_radius),
                loiter_orbit(loiter_orbit),
                heading(heading),
                elevation(elevation),
                turn_type(TURN_TYPE_STRAIGHT)
            {}

    /**
     * Construct move action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_NAV_WAYPOINT
     */
    Move_action(const mavlink::ugcs::Pld_mission_item_ex& item, Optional<double>& takeoff_altitude) :
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
        if (item->altitude_origin.Is_reset()) {
            takeoff_altitude = item->elevation;
        } else {
            takeoff_altitude = item->altitude_origin;
        }
        turn_type = static_cast<Turn_type>(item->turn_type.Get());
    }

    /**
     * Construct move action from protobuf command.
     */
    Move_action(const Property_list& p) :
        Action(Type::MOVE),
        position(Geodetic_tuple(0, 0, 0))
    {
        double lat = 0, lon = 0, alt = 0;
        auto pi = p.find("latitude");
        if (pi != p.end()) {
            pi->second->Get_value(lat);
        }
        pi = p.find("longitude");
        if (pi != p.end()) {
            pi->second->Get_value(lon);
        }
        pi = p.find("altitude_amsl");
        if (pi != p.end()) {
            pi->second->Get_value(alt);
        }
        position = Geodetic_tuple(lat, lon, alt);
        p.at("acceptance_radius")->Get_value(acceptance_radius);
        p.at("heading")->Get_value(heading);
        p.at("loiter_radius")->Get_value(loiter_orbit);
        p.at("wait_time")->Get_value(wait_time);
        p.at("ground_elevation")->Get_value(elevation);
        int t;
        p.at("turn_type")->Get_value(t);
        turn_type = static_cast<Turn_type>(t);
    }

    /**
     * Target global position of the movement.
     */
    Wgs84_position position;

    /**
     * Wait interval in seconds.
     * Shows how long the vehicle should stay/hold at the target position.
     */
    double wait_time = 0;

    /**
     * Acceptance radius of the target position in meters.
     * When the sphere with the radius centered at the target position is hit
     * by the vehicle the target position is considered reached.
     */
    double acceptance_radius = 0;

    /**
     * Radius of the point fly-by orbit in meters.
     * Positive value stands for the CW direction, negative - CCW.
     * Use {@code 0} for no-braking pass through the target point.
     */
    double loiter_orbit = 0;

    /**
     * Heading in radians.
     */
    double heading = 0;

    /**
     * Elevation in meters (i.e. terrain height) underneath the position.
     */
    double elevation = 0;

    Turn_type turn_type;
};

/** Type mapper from move action. */
template<>
struct Action::Mapper<Action::Type::MOVE> {
    /** Real type. */
    typedef Move_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _MOVE_ACTION_H_ */
