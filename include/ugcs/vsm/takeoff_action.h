// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file takeoff_action.h
 */
#ifndef _TAKEOFF_ACTION_H_
#define _TAKEOFF_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/optional.h>

namespace ugcs {
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
    Takeoff_action(const mavlink::ugcs::Pld_mission_item_ex& item, Optional<double>& takeoff_altitude) :
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
        if (item->altitude_origin.Is_reset()) {
            takeoff_altitude = item->elevation;
        } else {
            takeoff_altitude = item->altitude_origin;
        }
    }

    /**
     * Construct move action from protobuf command.
     */
    Takeoff_action(const Property_list& p) :
        Action(Type::TAKEOFF),
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
        p.at("climb_rate")->Get_value(climb_rate);
        p.at("ground_elevation")->Get_value(elevation);
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
} /* namespace ugcs */

#endif /* _TAKEOFF_ACTION_H_ */
