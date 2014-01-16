// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file set_home_action.h
 */
#ifndef _SET_HOME_ACTION_H_
#define _SET_HOME_ACTION_H_

#include <vsm/action.h>
#include <vsm/coordinates.h>
#include <vsm/mavlink.h>

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
     * Construct set home action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_DO_SET_HOME
     */
    Set_home_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::SET_HOME),
        use_current_position(item->param1 == 1),
        home_position(Geodetic_tuple(item->x * M_PI / 180.0,
                                     item->y * M_PI / 180.0,
                                     item->z)),
        elevation(item->elevation)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_DO_SET_HOME);
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

#endif /* _SET_HOME_ACTION_H_ */
