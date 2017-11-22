// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file set_servo_action.h
 */
#ifndef _SET_SERVO_ACTION_H_
#define _SET_SERVO_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Set_servo action. Move specified servo to given position.
 * Implementation depends on vehicle type. */
class Set_servo_action: public Action {
    DEFINE_COMMON_CLASS(Set_servo_action, Action)

public:
    /**
     * Construct wait action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_DO_SET_SERVO
     */
    Set_servo_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::SET_SERVO),
        servo_id(item->param1),
        pwm(item->param2)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_DO_SET_SERVO);
    }

    /**
     * Construct action from protobuf command.
     */
    Set_servo_action(const Property_list& p) :
        Action(Type::SET_SERVO)
    {
        p.at("pwm")->Get_value(pwm);
        p.at("servo_id")->Get_value(servo_id);
    }

    int servo_id;
    int pwm;
};

/** Type mapper for wait action. */
template<>
struct Action::Mapper<Action::Type::SET_SERVO> {
    /** Real type. */
    typedef Set_servo_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _SET_SERVO_ACTION_H_*/
