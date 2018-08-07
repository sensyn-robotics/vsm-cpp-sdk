// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file set_home_action.h
 */
#ifndef _UGCS_VSM_SET_HOME_ACTION_H_
#define _UGCS_VSM_SET_HOME_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Set home position action. */
class Set_home_action: public Action {
    DEFINE_COMMON_CLASS(Set_home_action, Action)

public:
    /** Construct set gome action explicitly. */
    Set_home_action(bool use_current_position, Wgs84_position home_position,
            double elevation) :
    Action(Type::SET_HOME),
    use_current_position(use_current_position),
    home_position(home_position),
    elevation(elevation) {}

    /**
     * Construct action from protobuf command.
     */
    Set_home_action(const Property_list& p) :
        Action(Type::SET_HOME),
        use_current_position(false),
        home_position(Geodetic_tuple(0, 0, 0))
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
        home_position = Geodetic_tuple(lat, lon, alt);
        p.at("ground_elevation")->Get_value(elevation);
    }

    /**
     * If set, current vehicle position should be set as the home/base position
     * on the action occurrence. Otherwise, vehicle should use @ref home_position
     * value.
     */
    bool use_current_position;

    /**
     * Global position, that should be used as the home/base position.
     */
    Wgs84_position home_position;

    /**
     * Elevation in meters (i.e. terrain height) underneath the home position.
     */
    double elevation;
};

/** Mapped for set home action. */
template<>
struct Action::Mapper<Action::Type::SET_HOME> {
    /** Real type. */
    typedef Set_home_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_SET_HOME_ACTION_H_ */
