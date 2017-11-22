// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/panorama_action.h>

using namespace ugcs::vsm;

Panorama_action::Trigger_state
Panorama_action::Mavlink_to_trigger_state(double mav_param)
{
    using Type = mavlink::ugcs::MAV_CAMERA_PANORAMA_MODE;

    switch (static_cast<unsigned>(mav_param)) {
    case Type::CAMERA_PANORAMA_MODE_VIDEO: return Trigger_state::ON;
    case Type::CAMERA_PANORAMA_MODE_PHOTO: return Trigger_state::SERIAL;
    }
    VSM_EXCEPTION(Action::Format_exception, "Trigger state value %d unknown",
            static_cast<unsigned>(mav_param));
}
