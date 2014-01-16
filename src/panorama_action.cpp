// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/panorama_action.h>

using namespace vsm;

Panorama_action::Trigger_state
Panorama_action::Mavlink_to_trigger_state(double mav_param)
{
    using Type = mavlink::ugcs::MAV_CAMERA_TRIGGER_STATE;

    switch (static_cast<unsigned>(mav_param)) {
    case Type::CAMERA_TRIGGER_STATE_ON: return Trigger_state::ON;
    case Type::CAMERA_TRIGGER_STATE_SERIAL_PHOTO: return Trigger_state::SERIAL;
    }
    VSM_EXCEPTION(Action::Format_exception, "Trigger state value %d unknown",
            static_cast<unsigned>(mav_param));
}
