// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file poi_action.h
 */
#ifndef _UGCS_VSM_POI_ACTION_H_
#define _UGCS_VSM_POI_ACTION_H_

#include <ugcs/vsm/action.h>

namespace ugcs {
namespace vsm {

/** Point of interest (POI) for a vehicle. */
class Poi_action:public Action {
    DEFINE_COMMON_CLASS(Poi_action, Action)

public:
    /** Construct POI explicitly. */
    Poi_action(Wgs84_position position, bool active) :
        Action(Type::POI),
        position(position), active(active) {}

    /**
     * Construct action from protobuf command.
     */
    Poi_action(const Property_list& p) :
        Action(Type::POI),
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
        p.at("active")->Get_value(active);
    }

    /** Position of a a POI. */
    Wgs84_position position;

    /** true - POI is active and should be monitored, false - POI is not active
     * and should not be anymore monitored. */
    bool active;
};

/** Type mapper for POI action. */
template<>
struct Action::Mapper<Action::Type::POI> {
    /** Real type. */
    typedef Poi_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_POI_ACTION_H_ */
