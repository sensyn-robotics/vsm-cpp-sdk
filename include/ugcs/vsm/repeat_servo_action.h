// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file set_servo_action.h
 */
#ifndef _REPEAT_SERVO_ACTION_H_
#define _REPEAT_SERVO_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Set_servo action. Move specified servo to given position given amount of times
 * Implementation depends on vehicle type. */
class Repeat_servo_action: public Action {
    DEFINE_COMMON_CLASS(Repeat_servo_action, Action)

public:
    /**
     * Construct wait action from Mavlink extended mission item.
     *
     * @param item With command equal to mavlink::MAV_CMD::MAV_CMD_DO_REPEAT_SERVO
     */
    Repeat_servo_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::REPEAT_SERVO),
        servo_id(item->param1),
        pwm(item->param2),
        count(item->param3),
        period(item->param4)
    {
        ASSERT(item->command == mavlink::MAV_CMD::MAV_CMD_DO_REPEAT_SERVO);
    }

    /**
     * Construct action from protobuf command.
     */
    Repeat_servo_action(const Property_list& p) :
        Action(Type::REPEAT_SERVO)
    {
        p.at("pwm")->Get_value(pwm);
        p.at("servo_id")->Get_value(servo_id);
        p.at("count")->Get_value(count);
        p.at("delay")->Get_value(period);
    }

    int servo_id;
    int pwm;
    int count;
    float period;
};

/** Type mapper for wait action. */
template<>
struct Action::Mapper<Action::Type::REPEAT_SERVO> {
    /** Real type. */
    typedef Repeat_servo_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _REPEAT_SERVO_ACTION_H_*/
