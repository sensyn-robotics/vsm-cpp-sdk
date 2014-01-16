// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file task_attributes_action.h
 */
#ifndef _TASK_ATTRIBUTES_ACTION_H_
#define _TASK_ATTRIBUTES_ACTION_H_

#include <vsm/action.h>
#include <vsm/mavlink.h>

namespace vsm {

/** Task attributes action. Initializes/changes task parameters, such as safety
 * configuration and emergency actions. */
class Task_attributes_action: public Action {
    DEFINE_COMMON_CLASS(Task_attributes_action, Action)

public:

    /** Action to perform in case of emergency. */
    enum Emergency_action {
        /** Return and land at the home position. */
        GO_HOME,
        /** Land at the current position. */
        LAND,
        /** Hover at the current position. */
        WAIT,
        /** Continue the task. */
        CONTINUE,
    };

    /** Construct task attributes action explicitly. */
    Task_attributes_action(double safe_altitude, Emergency_action rc_loss,
            Emergency_action gnss_loss, Emergency_action low_battery) :
                Action(Type::TASK_ATTRIBUTES),
                safe_altitude(safe_altitude), rc_loss(rc_loss),
                gnss_loss(gnss_loss), low_battery(low_battery) {}


    /**
     * Construct task attributes action from Mavlink mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_DO_SET_ROUTE_ATTRIBUTES
     */
    Task_attributes_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::TASK_ATTRIBUTES),
        safe_altitude(item->param1),
        rc_loss(Mavlink_to_emergency_action(item->param2)),
        gnss_loss(Mavlink_to_emergency_action(item->param3)),
        low_battery(Mavlink_to_emergency_action(item->param4))
    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_SET_ROUTE_ATTRIBUTES);
    }

public:

    /** Safe altitude in meters (MSL). */
    double safe_altitude;

    /** Type of action to perform in case of remote control loss. */
    Emergency_action rc_loss;

    /** Type of action to perform in case of GNSS signal loss (i.e. GPS, GLONASS
     * etc.).
     */
    Emergency_action gnss_loss;

    /** Type of action to perform in case of low battery. */
    Emergency_action low_battery;

private:

    /** Convert Mavlink emergency action value to Emergency_action enum.
     * @throw Format_exception is Mavlink value is unsupported.
     */
    static Emergency_action
    Mavlink_to_emergency_action(double mav_param);
};


/** Type mapper for task attributes action. */
template<>
struct Action::Mapper<Action::Type::TASK_ATTRIBUTES> {
    /** Real type. */
    typedef Task_attributes_action type;
};

} /* namespace vsm */

#endif /* _TASK_ATTRIBUTES_ACTION_H_ */
