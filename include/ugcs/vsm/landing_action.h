// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file landing_action.h
 * Landing action definition.
 */
#ifndef _UGCS_VSM_LANDING_ACTION_H_
#define _UGCS_VSM_LANDING_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Land at the specified position. */
class Landing_action: public Action {
    DEFINE_COMMON_CLASS(Landing_action, Action)

public:
    /** Construct the landing action explicitly. */
    Landing_action(Wgs84_position position, double heading, double elevation,
            double descend_rate, double acceptance_radius) :
    Action(Type::LANDING),
    position(position),
    heading(heading),
    elevation(elevation),
    descend_rate(descend_rate),
    acceptance_radius(acceptance_radius) {}

    /**
     * Construct move action from protobuf command.
     */
    Landing_action(const Property_list& p) :
        Action(Type::LANDING),
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
        p.at("descent_rate")->Get_value(descend_rate);
        p.at("ground_elevation")->Get_value(elevation);
    }

    /** Landing position. The landing phase should start at the specified
     * position, i.e. altitude is an initial altitude for the landing phase. */
    Wgs84_position position;

    /** Heading in radians. */
    double heading;

    /**
     * Elevation in meters (i.e. terrain height) underneath the position.
     */
    double elevation;

    /** Descending rate, m/s. */
    double descend_rate;

    /** Acceptance radius of the position. Maximum distance at which the position
     * is considered reached by the vehicle. Set in meters. */
    double acceptance_radius;
};

/** Type mapper for landing action. */
template<>
struct Action::Mapper<Action::Type::LANDING> {
    /** Real type. */
    typedef Landing_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_LANDING_ACTION_H_ */
