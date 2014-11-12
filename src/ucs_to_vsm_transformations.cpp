// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/ucs_to_vsm_transformations.h>
#include <ugcs/vsm/actions.h>

using namespace ugcs::vsm;

Action::Ptr
Ucs_to_vsm_transformations::Parse_mission_item_ex(
        const mavlink::ugcs::Pld_mission_item_ex& item,
        Optional<double>& takeoff_altitude)
{
    switch (item->command) {
    case mavlink::MAV_CMD_NAV_WAYPOINT:
        return Move_action::Create(item, takeoff_altitude);
    case mavlink::ugcs::MAV_CMD_NAV_LAND_EX:
        return Landing_action::Create(item);
    case mavlink::ugcs::MAV_CMD_NAV_TAKEOFF_EX:
        return Takeoff_action::Create(item, takeoff_altitude);
    case mavlink::MAV_CMD_CONDITION_DELAY:
        return Wait_action::Create(item);
    case mavlink::MAV_CMD_DO_CHANGE_SPEED:
        return Change_speed_action::Create(item);
    case mavlink::MAV_CMD_DO_SET_HOME:
        return Set_home_action::Create(item);
    case mavlink::MAV_CMD_NAV_ROI:
        return Poi_action::Create(item);
    case mavlink::MAV_CMD_CONDITION_YAW:
        return Heading_action::Create(item);
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_CONTROL:
        return Camera_control_action::Create(item);
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_TRIGGER:
        return Camera_trigger_action::Create(item);
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_PANORAMA:
         return Panorama_action::Create(item);
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_SET_ROUTE_ATTRIBUTES:
        return Task_attributes_action::Create(item);
    default:
        VSM_EXCEPTION(Unsupported_exception, "Mission item %u unsupported",
                item->command.Get());
    }
}
