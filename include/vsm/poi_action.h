// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file poi_action.h
 */
#ifndef _POI_ACTION_H_
#define _POI_ACTION_H_

#include <vsm/action.h>

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
     * Construct POI action from Mavlink mission item.
     * @throw Action::Format_exception if mission item has wrong format.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_NAV_ROI
     */
    Poi_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::POI),
        position(Geodetic_tuple(item->x * M_PI / 180.0,
                        item->y * M_PI / 180.0,
                        item->z)),
        active(item->param1 == mavlink::MAV_ROI::MAV_ROI_LOCATION ? true : false)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_NAV_ROI);
        if (item->param1 != mavlink::MAV_ROI::MAV_ROI_LOCATION &&
            item->param1 != mavlink::MAV_ROI::MAV_ROI_NONE) {
            VSM_EXCEPTION(Format_exception, "POI action unsupported type %f",
                                item->param1.Get());
        }
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

}; /* namespace vsm */

#endif /* _POI_ACTION_H_ */
