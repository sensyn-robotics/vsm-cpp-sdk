// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file camera_control_action.h
 */
#ifndef _CAMERA_CONTROL_ACTION_H_
#define _CAMERA_CONTROL_ACTION_H_

#include <vsm/action.h>
#include <vsm/mavlink.h>

namespace vsm {

/** Camera control action. Controls an on-board camera controller system. */
class Camera_control_action: public Action {
    DEFINE_COMMON_CLASS(Camera_control_action, Action)

public:

    /** Construct camera control action explicitly. */
    Camera_control_action(double tilt, double roll, double yaw, double zoom) :
        Action(Type::CAMERA_CONTROL),
        tilt(tilt), roll(roll), yaw(yaw), zoom(zoom) {}

    /**
     * Construct camera control action from Mavlink mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_CONTROL
     */
    Camera_control_action(const mavlink::ugcs::Pld_mission_item_ex& item) :
        Action(Type::CAMERA_CONTROL),
        tilt(item->param1 * M_PI / 180.0),
        roll(item->param2 * M_PI / 180.0),
        yaw(item->param3 * M_PI / 180.0),
        zoom(item->param4)
    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_CONTROL);
    }

    /** Target camera tilt value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for looking backward, 0 for full down and Pi/2 for looking straight forward.
     */
    double tilt;

    /** Target camera roll value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for left and Pi/2 for right.
     */
    double roll;

    /** Target camera yaw value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for left and Pi/2 for right.
     */
    double yaw;

    /** Device specific camera zoom level. */
    double zoom;
};

/** Type mapper for camera control action. */
template<>
struct Action::Mapper<Action::Type::CAMERA_CONTROL> {
    /** Real type. */
    typedef Camera_control_action type;
};

} /* namespace vsm */

#endif /* _CAMERA_CONTROL_ACTION_H_ */
