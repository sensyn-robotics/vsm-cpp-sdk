// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/task_attributes_action.h>

using namespace vsm;

Task_attributes_action::Emergency_action
Task_attributes_action::Mavlink_to_emergency_action(double mav_param)
{
    using Type = mavlink::ugcs::MAV_EMERGENCY_ACTION;

    switch (static_cast<unsigned>(mav_param)) {
    case Type::EMERGENCY_ACTION_GO_HOME: return Emergency_action::GO_HOME;
    case Type::EMERGENCY_ACTION_LAND: return Emergency_action::LAND;
    case Type::EMERGENCY_ACTION_WAIT: return Emergency_action::WAIT;
    case Type::EMERGENCY_ACTION_CONTINUE: return Emergency_action::CONTINUE;
    }
    VSM_EXCEPTION(Format_exception, "Emergency action type value %d unknown",
            static_cast<unsigned>(mav_param));
}
