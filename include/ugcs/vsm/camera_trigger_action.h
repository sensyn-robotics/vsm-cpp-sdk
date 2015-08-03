// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file camera_trigger_action.h
 */
#ifndef _CAMERA_TRIGGER_ACTION_H_
#define _CAMERA_TRIGGER_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** Camera trigger action. Changes on-board camera trigger state or triggering mode. */
class Camera_trigger_action: public Action {
    DEFINE_COMMON_CLASS(Camera_trigger_action, Action)

public:

    /** Camera trigger state. */
    enum State {
        /** Start recording. */
        ON,
        /** Stop recording or serial shooting. */
        OFF,
        /** Take a single photo. */
        SINGLE_PHOTO,
        /** Take a series of photos. */
        SERIAL_PHOTO
    };

    /** Construct camera trigger action explicitly. */
    Camera_trigger_action(State state, std::chrono::milliseconds interval):
        Action(Type::CAMERA_TRIGGER),
        state(state), interval(interval) {}

    /**
     * Construct camera trigger action from Mavlink mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_TRIGGER
     */

    Camera_trigger_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::CAMERA_TRIGGER),
        state(Mavlink_to_state(item->param1)),
        interval(1000) /* Currently not supported by protocol. */
    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_TRIGGER);
    }


    /** Camera trigger state. */
    State state;

    /** Interval between two consequent camera shots in case of serial triggering
     * (State::SERIAL_PHOTO).
     */
    std::chrono::milliseconds interval;

private:

    /** Convert camera trigger state parameter from Mavlink message to State
     * enum.
     * @throw Action::Format_exception if mav_param has unsupported value.
     */
    static State
    Mavlink_to_state(double mav_param);
};

/** Type mapper for camera trigger action. */
template<>
struct Action::Mapper<Action::Type::CAMERA_TRIGGER> {
    /** Real type. */
    typedef Camera_trigger_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _CAMERA_TRIGGER_ACTION_H_ */
