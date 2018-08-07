// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file camera_trigger_action.h
 */
#ifndef _UGCS_VSM_CAMERA_TRIGGER_ACTION_H_
#define _UGCS_VSM_CAMERA_TRIGGER_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** Camera trigger action. Changes on-board camera trigger state or triggering mode. */
class Camera_trigger_action: public Action {
    DEFINE_COMMON_CLASS(Camera_trigger_action, Action)

public:
    /** Construct camera trigger action explicitly. */
    Camera_trigger_action(proto::Camera_mission_trigger_state state, std::chrono::milliseconds interval):
        Action(Type::CAMERA_TRIGGER),
        state(state), interval(interval) {}

    /**
     * Construct action from protobuf command.
     */
    Camera_trigger_action(const Property_list& p) :
        Action(Type::CAMERA_TRIGGER),
        interval(1000) /* Currently not supported by protocol. */
    {
        int tmp;
        p.at("state")->Get_value(tmp);
        state = static_cast<proto::Camera_mission_trigger_state>(tmp);
    }

    /** Camera trigger state. */
    proto::Camera_mission_trigger_state state;

    /** Interval between two consequent camera shots in case of serial triggering
     * (State::SERIAL_PHOTO).
     */
    std::chrono::milliseconds interval;
};

/** Type mapper for camera trigger action. */
template<>
struct Action::Mapper<Action::Type::CAMERA_TRIGGER> {
    /** Real type. */
    typedef Camera_trigger_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_CAMERA_TRIGGER_ACTION_H_ */
