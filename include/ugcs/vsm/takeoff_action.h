// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file takeoff_action.h
 */
#ifndef _UGCS_VSM_TAKEOFF_ACTION_H_
#define _UGCS_VSM_TAKEOFF_ACTION_H_

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

#endif /* _UGCS_VSM_TAKEOFF_ACTION_H_ */
