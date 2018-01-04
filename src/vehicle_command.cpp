// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
#include <ugcs/vsm/vehicle_command.h>

using namespace ugcs::vsm;

Vehicle_command::Vehicle_command(Type type, const Property_list& p):
    type(type),
    position(Geodetic_tuple(0, 0, 0))
{
    int i;
    double lat = 0, lon = 0, alt = 0;
    auto pi = p.find("latitude");
    if (pi != p.end()) {
        pi->second->Get_value(lat);
    }
    pi = p.find("longitude");
    if (pi != p.end()) {
        pi->second->Get_value(lon);
    }
    pi = p.find("altitude_amsl");
    if (pi != p.end()) {
        pi->second->Get_value(alt);
    }
    position = Geodetic_tuple(lat, lon, alt);
    payload_id = 1; // legacy commands are always for payload 1.
    switch (type) {
    case Vehicle_command::Type::DIRECT_VEHICLE_CONTROL:
        p.at("roll")->Get_value(roll);
        p.at("pitch")->Get_value(pitch);
        p.at("yaw")->Get_value(yaw);
        p.at("throttle")->Get_value(throttle);
        break;
    case Vehicle_command::Type::DIRECT_PAYLOAD_CONTROL:
        p.at("roll")->Get_value(roll);
        p.at("pitch")->Get_value(pitch);
        p.at("yaw")->Get_value(yaw);
        p.at("zoom")->Get_value(zoom);
        break;
    case Vehicle_command::Type::CAMERA_POWER:
        if (p.at("power_state")->Is_value_na()) {
            power_state = Camera_power_state::UNKNOWN;
        } else {
            p.at("power_state")->Get_value(i);
            power_state = static_cast<Camera_power_state>(i);
        }
        break;
    case Vehicle_command::Type::CAMERA_TRIGGER:
        if (p.at("trigger_state")->Is_value_na()) {
            trigger_state = Camera_trigger_state::UNKNOWN;
        } else {
            p.at("trigger_state")->Get_value(i);
            trigger_state = static_cast<Camera_trigger_state>(i);
        }
        break;
    case Vehicle_command::Type::CAMERA_VIDEO_SOURCE:
        // only first source is supported.
        // Rework this once we have support for multiple cameras and video sources.
        payload_id = 1;
        break;
    case Type::WAYPOINT:
        p.at("acceptance_radius")->Get_value(acceptance_radius);
        p.at("altitude_origin")->Get_value(takeoff_altitude);
        p.at("ground_speed")->Get_value(speed);
        if (p.at("vertical_speed")->Is_value_na()) {
            vertical_speed = NAN;
        } else {
            p.at("vertical_speed")->Get_value(vertical_speed);
        }
        if (p.at("heading")->Is_value_na()) {
            heading = NAN;
        } else {
            p.at("heading")->Get_value(heading);
        }
        break;
    default:
        // Other commands do not have parameters.
        break;
    }
}

/** Construct command with parameters. */
Vehicle_command::Vehicle_command(Type type, const mavlink::ugcs::Pld_command_long_ex& cmd):
    type(type),
    position(Geodetic_tuple(cmd->param5 * M_PI / 180.0,
                    cmd->param6 * M_PI / 180.0,
                    cmd->param7))
{
    switch (type) {
    case Vehicle_command::Type::DIRECT_VEHICLE_CONTROL:
        roll = cmd->param1.Get();
        pitch = cmd->param2.Get();
        yaw = cmd->param3.Get();
        throttle = cmd->param4.Get();
        break;
    case Vehicle_command::Type::DIRECT_PAYLOAD_CONTROL:
        roll = cmd->param1.Get();
        pitch = cmd->param2.Get();
        yaw = cmd->param3.Get();
        zoom = cmd->param4.Get();
        payload_id = cmd->param7.Get();
        break;
    case Vehicle_command::Type::CAMERA_VIDEO_SOURCE:
        payload_id = cmd->param7.Get();
        break;
    case Vehicle_command::Type::CAMERA_POWER:
        payload_id = cmd->param7.Get();
        if (cmd->param1.Is_reset()) {
            power_state = Camera_power_state::UNKNOWN;
        } else {
            switch (static_cast<mavlink::ugcs::MAV_PAYLOAD_POWER_STATE>(cmd->param1.Get())) {
            case mavlink::ugcs::MAV_PAYLOAD_POWER_STATE_OFF:
                power_state = Camera_power_state::OFF;
                break;
            case mavlink::ugcs::MAV_PAYLOAD_POWER_STATE_ON:
                power_state = Camera_power_state::ON;
                break;
            case mavlink::ugcs::MAV_PAYLOAD_POWER_STATE_TOGGLE:
                power_state = Camera_power_state::TOGGLE;
                break;
            }
        }
        break;
    case Vehicle_command::Type::CAMERA_TRIGGER:
        payload_id = cmd->param7.Get();
        if (cmd->param1.Is_reset()) {
            trigger_state = Camera_trigger_state::UNKNOWN;
        } else {
            switch (static_cast<mavlink::ugcs::MAV_CAMERA_TRIGGER_STATE>(cmd->param1.Get())) {
            case mavlink::ugcs::CAMERA_TRIGGER_STATE_SINGLE_SHOT:
                trigger_state = Camera_trigger_state::SINGLE_SHOT;
                break;
            case mavlink::ugcs::CAMERA_TRIGGER_STATE_START_RECORDING:
                trigger_state = Camera_trigger_state::VIDEO_START;
                break;
            case mavlink::ugcs::CAMERA_TRIGGER_STATE_STOP_RECORDING:
                trigger_state = Camera_trigger_state::VIDEO_STOP;
                break;
            case mavlink::ugcs::CAMERA_TRIGGER_STATE_TOGGLE_RECORDING:
                trigger_state = Camera_trigger_state::VIDEO_TOGGLE;
                break;
            }
        }
        break;
    case Type::WAYPOINT:
        acceptance_radius = cmd->param1.Get();
        takeoff_altitude = cmd->param2.Get();
        speed = cmd->param3.Get();
        heading = cmd->param4 * M_PI / 180.0;
        break;
    default:
        // Other commands do not have parameters.
        break;
    }
}
