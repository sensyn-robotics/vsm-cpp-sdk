// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file camera_control_action.h
 */
#ifndef _UGCS_VSM_CAMERA_CONTROL_ACTION_H_
#define _UGCS_VSM_CAMERA_CONTROL_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
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
     * Construct action from protobuf command.
     */
    Camera_control_action(const Property_list& p) :
        Action(Type::CAMERA_CONTROL)
    {
        has_tilt = p.at("tilt")->Get_value(tilt);
        has_roll = p.at("roll")->Get_value(roll);
        has_yaw  = p.at("yaw")->Get_value(yaw);
        has_zoom = p.at("zoom_level")->Get_value(zoom);
    }

    /** Target camera tilt value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for looking backward, 0 for full down and Pi/2 for looking straight forward.
     */
    double tilt;
    bool has_tilt;

    /** Target camera roll value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for left and Pi/2 for right.
     */
    double roll;
    bool has_roll;

    /** Target camera yaw value in radians: [-Pi/2, Pi/2], where -Pi/2 stands
     * for left and Pi/2 for right.
     */
    double yaw;
    bool has_yaw;

    /** Device specific camera zoom level. */
    double zoom;
    bool has_zoom;
};

/** Type mapper for camera control action. */
template<>
struct Action::Mapper<Action::Type::CAMERA_CONTROL> {
    /** Real type. */
    typedef Camera_control_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_CAMERA_CONTROL_ACTION_H_ */
