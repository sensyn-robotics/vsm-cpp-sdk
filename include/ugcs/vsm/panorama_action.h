// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file panorama_action.h
 */
#ifndef _UGCS_VSM_PANORAMA_ACTION_H_
#define _UGCS_VSM_PANORAMA_ACTION_H_

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
    /** Construct panorama action explicitly. */
    Panorama_action(proto::Panorama_mode trigger_state, double angle, double step,
                    std::chrono::milliseconds delay, double speed):
        Action(Type::PANORAMA), trigger_state(trigger_state),
        angle(angle), step(step), delay(delay), speed(speed)
        {}

    /**
     * Construct action from protobuf command.
     */
    Panorama_action(const Property_list& p) :
        Action(Type::PANORAMA)
    {
        int tmp;
        float time;
        p.at("mode")->Get_value(tmp);
        trigger_state = static_cast<proto::Panorama_mode>(tmp);
        p.at("angle")->Get_value(angle);
        p.at("step")->Get_value(step);
        p.at("delay")->Get_value(time);
        tmp = time * 1000;
        delay = std::chrono::milliseconds(tmp);
        p.at("speed")->Get_value(speed);
    }

    /** Trigger state. */
    proto::Panorama_mode trigger_state;

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
};

/** Type mapper for panorama action. */
template<>
struct Action::Mapper<Action::Type::PANORAMA> {
    /** Real type. */
    typedef Panorama_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_PANORAMA_ACTION_H_ */
