// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file panorama_action.h
 */
#ifndef _PANORAMA_ACTION_H_
#define _PANORAMA_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** Panorama action. Makes a panoramic shot. Target system rotates about its
 * yaw axis in order to reach the target angle. Shooting is performed either in
 * continuous (permanent exposure) or discrete (shoot per step) mode.
 * Continuous shooting rotation can still be performed in serial steps with
 * delays.
 */
class Panorama_action: public Action {
    DEFINE_COMMON_CLASS(Panorama_action, Action)

public:

    /** Camera trigger state. */
    enum Trigger_state {
        /** Continuous exposure. */
        ON,
        /** Discrete, once per step, shooting. */
        SERIAL
    };

    /** Construct panorama action explicitly. */
    Panorama_action(Trigger_state trigger_state, double angle, double step,
                    std::chrono::milliseconds delay, double speed):
        Action(Type::PANORAMA), trigger_state(trigger_state),
        angle(angle), step(step), delay(delay), speed(speed)
        {}

    /**
     * Construct panorama action action from Mavlink mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_DO_PANORAMA
     */
    Panorama_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::PANORAMA),
        trigger_state(Mavlink_to_trigger_state(item->param1)),
        angle(item->param2 * M_PI / 180.0),
        step(item->param3 * M_PI / 180.0),
        delay(std::chrono::milliseconds(item->param4)),
        speed(item->x * M_PI / 180.0)

    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_PANORAMA);
    }

    /**
     * Construct action from protobuf command.
     */
    Panorama_action(const Property_list& p) :
        Action(Type::PANORAMA)
    {
        int tmp;
        float time;
        p.at("mode")->Get_value(tmp);
        trigger_state = Mavlink_to_trigger_state(tmp);
        p.at("angle")->Get_value(angle);
        p.at("step")->Get_value(step);
        p.at("delay")->Get_value(time);
        tmp = time * 1000;
        delay = std::chrono::milliseconds(tmp);
        p.at("speed")->Get_value(speed);
    }

    /** Trigger state. */
    Trigger_state trigger_state;

    /** Target panorama angle in a range [-2Pi, 2Pi]. If positive the
     * rotation direction should be clockwise, if negative the rotation
     * direction should be counter-clockwise. Set in radians.
     * */
    double angle;

    /** Absolute value of a step angle in case of a discrete shooting.
     * Zero stands for a continuous rotation. Set in radians.
     */
    double step;

    /** Delay interval between two steps. */
    std::chrono::milliseconds delay;

    /** Rotation speed in radians per second. */
    double speed;

private:

    /** Convert trigger state parameter from Mavlink message to Trigger_state
     * enum.
     * @throw Action::Format_exception if mav_param has unsupported value.
     */
    static Trigger_state
    Mavlink_to_trigger_state(double mav_param);


};

/** Type mapper for panorama action. */
template<>
struct Action::Mapper<Action::Type::PANORAMA> {
    /** Real type. */
    typedef Panorama_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _PANORAMA_ACTION_H_ */
