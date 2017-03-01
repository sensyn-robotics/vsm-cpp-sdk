// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Vehicle class implementation.
 */

#include <ugcs/vsm/vehicle.h>
#include <ugcs/vsm/log.h>
#include <ugcs/vsm/actions.h>

#include <iostream>

using namespace ugcs::vsm;

std::hash<Vehicle*> Vehicle::Hasher::hasher;

std::mutex Vehicle::state_mutex;

Vehicle::Vehicle(mavlink::MAV_TYPE type, mavlink::MAV_AUTOPILOT autopilot,
        const Capabilities& capabilities,
        const std::string& serial_number,
        const std::string& model_name,
        int model_name_is_hardcoded,
        bool create_thread) :
            Device(create_thread),
            serial_number(serial_number),
            model_name(model_name),
            type(type),
            autopilot(autopilot),
            model_name_is_hardcoded(model_name_is_hardcoded)
{
    switch (type) {
    case mavlink::MAV_TYPE_HEXAROTOR:
        frame_type = "generic_hexa_x";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER;
        break;
    case mavlink::MAV_TYPE_OCTOROTOR:
        frame_type = "generic_octa_v";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER;
        break;
    case mavlink::MAV_TYPE_QUADROTOR:
    case mavlink::MAV_TYPE_VTOL_QUADROTOR:
        frame_type = "generic_quad_x";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER;
        break;
    case mavlink::MAV_TYPE_TRICOPTER:
        frame_type = "generic_tri_y";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER;
        break;
    case mavlink::MAV_TYPE_VTOL_DUOROTOR:
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER;
        break;
    case mavlink::MAV_TYPE_HELICOPTER:
    case mavlink::MAV_TYPE_COAXIAL:
        frame_type = "generic_heli";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_HELICOPTER;
        break;
    case mavlink::MAV_TYPE_FIXED_WING:
    case mavlink::MAV_TYPE_AIRSHIP:
        frame_type = "generic_fixed_wing";
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_FIXED_WING;
        break;
    case mavlink::MAV_TYPE_GROUND_ROVER:
        vehicle_type = ugcs::vsm::proto::VEHICLE_TYPE_GROUND;
        break;
    default:
        VSM_EXCEPTION(Exception, "Unsupported vehicle mav_type: %d", type);
    }
    Property::Ptr prop;

// Create flight controller
    auto fc = Add_subdevice(SUBDEVICE_TYPE_FC);
    subsystems.fc = fc;

    // Create telemetry
#define ADD_T(x) t_##x = Add_telemetry(fc, #x);
#define ADD_TS(x, y) t_##x = Add_telemetry(fc, #x, y);

    ADD_TS(altitude_origin, proto::FIELD_SEMANTIC_ALTITUDE_AMSL);
    ADD_TS(is_armed, proto::FIELD_SEMANTIC_BOOL);
    ADD_T(vertical_speed);
    ADD_T(control_mode);
    ADD_T(main_current);
    ADD_TS(downlink_present, proto::FIELD_SEMANTIC_BOOL);
    ADD_T(gcs_link_quality);
    ADD_T(satellite_count);
    ADD_T(gps_fix);
    ADD_T(rc_link_quality);
    ADD_TS(uplink_present, proto::FIELD_SEMANTIC_BOOL);
    ADD_T(altitude_raw);
    ADD_T(altitude_amsl);
    ADD_T(air_speed);
    ADD_T(course);
    ADD_T(ground_speed);
    ADD_T(heading);
    ADD_T(latitude);
    ADD_T(longitude);
    ADD_T(pitch);
    ADD_T(roll);
    ADD_T(main_voltage);

    ADD_TS(home_latitude, proto::FIELD_SEMANTIC_LATITUDE);
    ADD_TS(home_longitude, proto::FIELD_SEMANTIC_LONGITUDE);
    ADD_TS(home_altitude_amsl, proto::FIELD_SEMANTIC_ALTITUDE_AMSL);
    ADD_TS(home_altitude_raw, proto::FIELD_SEMANTIC_ALTITUDE_RAW);

    // Create command definitions.

    c_arm = Add_command(fc, "arm", true, false);

    c_auto = Add_command(fc, "auto", true, false);

    c_direct_vehicle_control = Add_command(fc, "direct_vehicle_control", true, false);
    prop = c_direct_vehicle_control->Add_parameter("pitch", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_vehicle_control->Add_parameter("roll", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_vehicle_control->Add_parameter("yaw", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_vehicle_control->Add_parameter("throttle", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);

    c_disarm = Add_command(fc, "disarm", true, false);

    c_emergency_land = Add_command(fc, "emergency_land", true, false);

    c_guided = Add_command(fc, "guided", true, false);

    c_joystick = Add_command(fc, "joystick", true, false);

    c_land_command = Add_command(fc, "land_command", true, false);

    c_manual = Add_command(fc, "manual", true, false);

    c_mission_upload = Add_command(fc, "mission_upload", true, false);
    c_mission_upload->Add_parameter("altitude_origin");
    c_mission_upload->Add_parameter("safe_altitude", proto::FIELD_SEMANTIC_ALTITUDE_AMSL);

    // Derived vehicle should set supported enum values for these actions
    // possibly from proto::Failsafe_action
    p_rc_loss_action = c_mission_upload->Add_parameter("rc_loss_action", Property::VALUE_TYPE_ENUM);
    p_gps_loss_action = c_mission_upload->Add_parameter("gps_loss_action", Property::VALUE_TYPE_ENUM);
    p_low_battery_action = c_mission_upload->Add_parameter("low_battery_action", Property::VALUE_TYPE_ENUM);
    // Additional parameters should be added by derived classes.

    c_pause = Add_command(fc, "mission_pause", true, false);

    c_resume = Add_command(fc, "mission_resume", true, false);

    c_rth = Add_command(fc, "return_to_home", true, false);

    c_takeoff_command = Add_command(fc, "takeoff_command", true, false);

    c_waypoint = Add_command(fc, "waypoint", true, false);
    c_waypoint->Add_parameter("latitude");
    c_waypoint->Add_parameter("longitude");
    c_waypoint->Add_parameter("altitude_amsl");
    c_waypoint->Add_parameter("acceptance_radius");
    c_waypoint->Add_parameter("altitude_origin");
    c_waypoint->Add_parameter("ground_speed");
    c_waypoint->Add_parameter("vertical_speed");
    c_waypoint->Add_parameter("heading");

// Legacy mission items

    c_move = Add_command(fc, "move", false, true);
    p_wp_turn_type = c_move->Add_parameter("turn_type", Property::VALUE_TYPE_ENUM);
    p_wp_turn_type->Add_enum("stop_and_turn", proto::TURN_TYPE_STOP_AND_TURN);
    p_wp_turn_type->Add_enum("straight", proto::TURN_TYPE_STRAIGHT);
    p_wp_turn_type->Add_enum("spline", proto::TURN_TYPE_SPLINE);
    p_wp_turn_type->Add_enum("bank_turn", proto::TURN_TYPE_BANK_TURN);
    c_move->Add_parameter("latitude");
    c_move->Add_parameter("longitude");
    c_move->Add_parameter("altitude_amsl");
    c_move->Add_parameter("acceptance_radius");
    c_move->Add_parameter("loiter_radius", Property::VALUE_TYPE_FLOAT);
    c_move->Add_parameter("wait_time", Property::VALUE_TYPE_FLOAT);
    c_move->Add_parameter("heading");
    c_move->Add_parameter("ground_elevation");

    c_wait = Add_command(fc, "wait", false, true);
    c_wait->Add_parameter("time", Property::VALUE_TYPE_FLOAT);

    c_set_speed = Add_command(fc, "set_speed", false, true);
    c_set_speed->Add_parameter("ground_speed");
    c_set_speed->Add_parameter("vertical_speed");

    c_set_home = Add_command(fc, "set_home", false, true);
    c_set_home->Add_parameter("latitude");
    c_set_home->Add_parameter("longitude");
    c_set_home->Add_parameter("altitude_amsl");
    c_set_home->Add_parameter("ground_elevation");

    c_set_poi = Add_command(fc, "set_poi", false, true);
    c_set_poi->Add_parameter("latitude");
    c_set_poi->Add_parameter("longitude");
    c_set_poi->Add_parameter("altitude_amsl");
    c_set_poi->Add_parameter("active", Property::VALUE_TYPE_BOOL);

    c_set_heading = Add_command(fc, "set_heading", false, true);
    c_set_heading->Add_parameter("heading");

    c_panorama = Add_command(fc, "panorama", false, true);
    prop = c_panorama->Add_parameter("mode", Property::VALUE_TYPE_ENUM);
    prop->Add_enum("photo", proto::PANORAMA_MODE_PHOTO);
    prop->Add_enum("video", proto::PANORAMA_MODE_VIDEO);
    c_panorama->Add_parameter("angle", Property::VALUE_TYPE_FLOAT);
    c_panorama->Add_parameter("step", Property::VALUE_TYPE_FLOAT);
    c_panorama->Add_parameter("delay", Property::VALUE_TYPE_FLOAT);
    c_panorama->Add_parameter("speed", Property::VALUE_TYPE_FLOAT);

    c_takeoff_mission = Add_command(fc, "takeoff_mission", false, true);
    c_takeoff_mission->Add_parameter("latitude");
    c_takeoff_mission->Add_parameter("longitude");
    c_takeoff_mission->Add_parameter("altitude_amsl");
    c_takeoff_mission->Add_parameter("acceptance_radius");
    c_takeoff_mission->Add_parameter("heading");
    c_takeoff_mission->Add_parameter("climb_rate");
    c_takeoff_mission->Add_parameter("ground_elevation");

    c_land_mission = Add_command(fc, "land_mission", false, true);
    c_land_mission->Add_parameter("latitude");
    c_land_mission->Add_parameter("longitude");
    c_land_mission->Add_parameter("altitude_amsl");
    c_land_mission->Add_parameter("acceptance_radius");
    c_land_mission->Add_parameter("heading");
    c_land_mission->Add_parameter("descent_rate");
    c_land_mission->Add_parameter("ground_elevation");

// Create primary camera. (Derived vehicles can add other cameras and/or add new commands to this one)
    auto cam = Add_subdevice(SUBDEVICE_TYPE_CAMERA);
    subsystems.camera = cam;

    c_camera_video_source = Add_command(cam, "select_as_video_source", true, false);

    c_camera_power = Add_command(cam, "camera_power", true, false);
    prop = c_camera_power->Add_parameter("power_state", Property::VALUE_TYPE_ENUM);
    prop->Add_enum("on", static_cast<int>(proto::CAMERA_POWER_STATE_ON));
    prop->Add_enum("off", static_cast<int>(proto::CAMERA_POWER_STATE_OFF));
    prop->Add_enum("toggle", static_cast<int>(proto::CAMERA_POWER_STATE_TOGGLE));

    c_camera_trigger_command = Add_command(cam, "camera_trigger_command", true, false);
    prop = c_camera_trigger_command->Add_parameter("trigger_state", Property::VALUE_TYPE_ENUM);
    prop->Add_enum("single_shot", static_cast<int>(proto::CAMERA_COMMAND_TRIGGER_STATE_SINGLE_SHOT));
    prop->Add_enum("video_start", static_cast<int>(proto::CAMERA_COMMAND_TRIGGER_STATE_VIDEO_START));
    prop->Add_enum("video_stop", static_cast<int>(proto::CAMERA_COMMAND_TRIGGER_STATE_VIDEO_STOP));
    prop->Add_enum("video_toggle", static_cast<int>(proto::CAMERA_COMMAND_TRIGGER_STATE_VIDEO_TOGGLE));

    c_camera_trigger_mission = Add_command(cam, "camera_trigger_mission", false, true);
    prop = c_camera_trigger_mission->Add_parameter("state", Property::VALUE_TYPE_ENUM);
    prop->Add_enum("off", static_cast<int>(proto::CAMERA_MISSION_TRIGGER_STATE_OFF));
    prop->Add_enum("on", static_cast<int>(proto::CAMERA_MISSION_TRIGGER_STATE_ON));
    prop->Add_enum("serial_photo", static_cast<int>(proto::CAMERA_MISSION_TRIGGER_STATE_SERIAL_PHOTO));
    prop->Add_enum("single_photo", static_cast<int>(proto::CAMERA_MISSION_TRIGGER_STATE_SINGLE_PHOTO));

    c_camera_by_distance = Add_command(cam, "camera_trigger_by_distance", false, true);
    c_camera_by_distance->Add_parameter("distance", Property::VALUE_TYPE_FLOAT);
    c_camera_by_distance->Add_parameter("count", Property::VALUE_TYPE_INT);
    c_camera_by_distance->Add_parameter("delay", Property::VALUE_TYPE_FLOAT);

    c_camera_by_time = Add_command(cam, "camera_trigger_by_time", false, true);
    c_camera_by_time->Add_parameter("period", Property::VALUE_TYPE_FLOAT);
    c_camera_by_time->Add_parameter("count", Property::VALUE_TYPE_INT);
    c_camera_by_time->Add_parameter("delay", Property::VALUE_TYPE_FLOAT);

// Create gimbal.
    auto gimbal = Add_subdevice(SUBDEVICE_TYPE_GIMBAL);
    subsystems.gimbal = gimbal;

    c_direct_payload_control = Add_command(gimbal, "direct_payload_control", true, false);
    prop = c_direct_payload_control->Add_parameter("pitch", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_payload_control->Add_parameter("roll", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_payload_control->Add_parameter("yaw", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);
    prop = c_direct_payload_control->Add_parameter("zoom", Property::VALUE_TYPE_FLOAT);
    prop->Max_value()->Set_value(1);
    prop->Min_value()->Set_value(-1);

    Set_capabilities(capabilities); // this should be after all commands are created.
    Calculate_system_id();
}

void
Vehicle::Reset_altitude_origin()
{
    CREATE_COMMIT_SCOPE;
    if (current_altitude_origin) {
        t_altitude_origin->Set_value(*current_altitude_origin);
    } else {
        t_altitude_origin->Set_value_na();
    }
    t_altitude_origin->Set_changed();   // Force sending even if value has not changed.
}

void
Vehicle::Set_altitude_origin(float altitude_amsl)
{
    CREATE_COMMIT_SCOPE;
    current_altitude_origin = altitude_amsl;
    t_altitude_origin->Set_value(altitude_amsl);
    t_altitude_origin->Set_changed();   // Force sending even if value has not changed.
}

void
Vehicle::Handle_ucs_command(
    Ucs_request::Ptr ucs_request)
{
    if (ucs_request->request.device_commands_size() != 1) {
        LOG_ERR("Only one command allowed in ucs message, got %d", ucs_request->request.device_commands_size());
        ucs_request->Complete(
            ugcs::vsm::proto::STATUS_FAILED,
            "Only one command allowed in ucs message");
        return;
    }

    auto completion_handler = Make_callback(
        &Vehicle::Command_completed,
        Shared_from_this(),
        Vehicle_request::Result::NOK,
        std::string(),
        ucs_request);

    auto &vsm_cmd = ucs_request->request.device_commands(0);
    auto cmd = Get_command(vsm_cmd.command_id());
    Property_list params;
    if (cmd) {
        VEHICLE_LOG_INF((*this), "COMMAND %s (%d) received",
            cmd->Get_name().c_str(),
            vsm_cmd.command_id());
    }

    Vehicle_task_request::Ptr task = nullptr;

    try {
        params = cmd->Build_parameter_list(vsm_cmd);

        if (cmd == c_mission_upload) {
            task = Vehicle_task_request::Create(
                completion_handler,
                completion_ctx,
                vsm_cmd.sub_commands_size());

            float altitude_origin;
            if (params.at("altitude_origin")->Get_value(altitude_origin)) {
                LOG("Altitude origin: %f", altitude_origin);
                task->payload.Set_takeoff_altitude(altitude_origin);
            } else {
                VSM_EXCEPTION(Action::Format_exception, "Altitude origin not present in mission");
            }

            task->payload.attributes = Task_attributes_action::Create(params);

            auto item_count = 0;
            for (int i = 0; i < vsm_cmd.sub_commands_size(); i++) {
                auto vsm_scmd = vsm_cmd.sub_commands(i);
                auto cmd = Get_command(vsm_scmd.command_id());
                if (cmd) {
                    item_count++;
                    VEHICLE_LOG_INF((*this),
                        "MISSION item %d %s (%d)",
                        item_count, cmd->Get_name().c_str(), vsm_scmd.command_id());
                    params = cmd->Build_parameter_list(vsm_scmd);
                    if (!cmd->Is_mission_item()) {
                        VSM_EXCEPTION(Action::Format_exception, "Command not allowed in mission");
                    }
                    float tmp = std::nanf("");
                    if (cmd == c_set_parameter) {
                        // Used only by arduplane params ("LANDING_FLARE_TIME" and friends)
                        task->payload.parameters = std::move(params);
                    } else if (cmd == c_move) {
                        task->payload.actions.push_back(Move_action::Create(params));
                    } else if (cmd == c_land_mission) {
                        task->payload.actions.push_back(Landing_action::Create(params));
                    } else if (cmd == c_takeoff_mission) {
                        task->payload.actions.push_back(Takeoff_action::Create(params));
                    } else if (cmd == c_wait) {
                        params.at("time")->Get_value(tmp);
                        task->payload.actions.push_back(Wait_action::Create(tmp));
                    } else if (cmd == c_set_speed) {
                        task->payload.actions.push_back(Change_speed_action::Create(params));
                    } else if (cmd == c_set_home) {
                        task->payload.actions.push_back(Set_home_action::Create(params));
                    } else if (cmd == c_set_poi) {
                        task->payload.actions.push_back(Poi_action::Create(params));
                    } else if (cmd == c_set_heading) {
                        params.at("heading")->Get_value(tmp);
                        task->payload.actions.push_back(Heading_action::Create(tmp));
                    } else if (cmd == c_panorama) {
                        task->payload.actions.push_back(Panorama_action::Create(params));
                    } else if (cmd == c_camera_trigger_mission) {
                        task->payload.actions.push_back(Camera_trigger_action::Create(params));
                    } else if (cmd == c_camera_by_time) {
                        task->payload.actions.push_back(Camera_series_by_time_action::Create(params));
                    } else if (cmd == c_camera_by_distance) {
                        task->payload.actions.push_back(Camera_series_by_distance_action::Create(params));
                    } else if (cmd == c_payload_control) {
                        task->payload.actions.push_back(Camera_control_action::Create(params));
                    } else {
                        VSM_EXCEPTION(Action::Format_exception, "Unsupported mission item '%s'", cmd->Get_name().c_str());
                    }
                } else {
                    VSM_EXCEPTION(Action::Format_exception, "Unregistered mission item %d", vsm_scmd.command_id());
                }
            }
            Submit_vehicle_request(task);
        } else if (cmd) {
            Vehicle_command::Type ctype;
            if (cmd == c_arm) {
                ctype = Vehicle_command::Type::ARM;
            } else if (cmd == c_auto) {
                ctype = Vehicle_command::Type::AUTO_MODE;
            } else if (cmd == c_direct_vehicle_control) {
                ctype = Vehicle_command::Type::DIRECT_VEHICLE_CONTROL;
            } else if (cmd == c_disarm) {
                ctype = Vehicle_command::Type::DISARM;
            } else if (cmd == c_guided) {
                ctype = Vehicle_command::Type::GUIDED_MODE;
            } else if (cmd == c_joystick) {
                ctype = Vehicle_command::Type::JOYSTICK_CONTROL_MODE;
            } else if (cmd == c_land_command) {
                ctype = Vehicle_command::Type::LAND;
            } else if (cmd == c_takeoff_command) {
                ctype = Vehicle_command::Type::TAKEOFF;
            } else if (cmd == c_manual) {
                ctype = Vehicle_command::Type::MANUAL_MODE;
            } else if (cmd == c_pause) {
                ctype = Vehicle_command::Type::PAUSE_MISSION;
            } else if (cmd == c_resume) {
                ctype = Vehicle_command::Type::RESUME_MISSION;
            } else if (cmd == c_rth) {
                ctype = Vehicle_command::Type::RETURN_HOME;
            } else if (cmd == c_waypoint) {
                ctype = Vehicle_command::Type::WAYPOINT;
            } else if (cmd == c_emergency_land) {
                ctype = Vehicle_command::Type::EMERGENCY_LAND;
            } else if (cmd == c_adsb_install) {
                ctype = Vehicle_command::Type::ADSB_INSTALL;
            } else if (cmd == c_adsb_preflight) {
                ctype = Vehicle_command::Type::ADSB_PREFLIGHT;
            } else if (cmd == c_adsb_operating) {
                ctype = Vehicle_command::Type::ADSB_OPERATING;
            } else if (cmd == c_camera_trigger_command) {
                ctype = Vehicle_command::Type::CAMERA_TRIGGER;
            } else if (cmd == c_direct_payload_control) {
                ctype = Vehicle_command::Type::DIRECT_PAYLOAD_CONTROL;
            } else if (cmd == c_camera_power) {
                ctype = Vehicle_command::Type::CAMERA_POWER;
            } else if (cmd == c_camera_video_source) {
                ctype = Vehicle_command::Type::CAMERA_VIDEO_SOURCE;
            } else {
                ucs_request->Complete(ugcs::vsm::proto::STATUS_INVALID_COMMAND, "Unsupported command. Only legacy commands supported for now.");
                return;
            }
            auto task = Vehicle_command_request::Create(completion_handler, completion_ctx, ctype, params);
            Submit_vehicle_request(task);
        } else {
            ucs_request->Complete(ugcs::vsm::proto::STATUS_INVALID_COMMAND, "Unknown command id");
        }
    } catch (const std::exception& ex) {
        ucs_request->Complete(ugcs::vsm::proto::STATUS_INVALID_COMMAND, ex.what());
        if (task) {
            task->Abort();
        }
    }
}

void
Vehicle::Fill_register_msg(ugcs::vsm::proto::Vsm_message& msg)
{
    auto dev = msg.mutable_register_device();

    dev->set_begin_of_epoch(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            begin_of_epoch.time_since_epoch()).count());

    auto reg = dev->mutable_register_vehicle();


    reg->set_vehicle_type(vehicle_type);
    if (!serial_number.empty()) {
        reg->set_vehicle_serial(serial_number);
    }
    if (!model_name.empty()) {
        reg->set_vehicle_name(model_name);
    }
    if (!port_name.empty()) {
        reg->set_port_name(port_name);
    }
    if (!frame_type.empty()) {
        reg->set_frame_type(frame_type);
    }

    for (auto p : properties) {
        p.second->Write_as_property(dev->add_properties());
    }

    for (auto subdevice: subdevices) {
        switch (subdevice.type) {
        case SUBDEVICE_TYPE_FC:
        {
            auto sd = reg->add_register_flight_controller();
            sd->set_autopilot_serial(autopilot_serial);
            sd->set_autopilot_type(autopilot_type);
            for (auto it: subdevice.telemetry_fields) {
                it.second->Register(sd->add_telemetry_fields());
            }
            for (auto it: subdevice.commands) {
                it.second->Register(sd->add_commands());
            }
        }
        break;
        case SUBDEVICE_TYPE_CAMERA:
        {
            auto sd = reg->add_register_camera();
            for (auto it: subdevice.telemetry_fields) {
                it.second->Register(sd->add_telemetry_fields());
            }
            for (auto it: subdevice.commands) {
                it.second->Register(sd->add_commands());
            }
        }
        break;
        case SUBDEVICE_TYPE_ADSB_TRANSPONDER:
        {
            auto sd = reg->add_register_adsb_transponder();
            for (auto it: subdevice.telemetry_fields) {
                it.second->Register(sd->add_telemetry_fields());
            }
            for (auto it: subdevice.commands) {
                it.second->Register(sd->add_commands());
            }
        }
        break;
        case SUBDEVICE_TYPE_GIMBAL:
        {
            auto sd = reg->add_register_gimbal();
            for (auto it: subdevice.telemetry_fields) {
                it.second->Register(sd->add_telemetry_fields());
            }
            for (auto it: subdevice.commands) {
                it.second->Register(sd->add_commands());
            }
        }
        break;
        }
    }
    //LOG("Register msg:%s", msg.DebugString().c_str());
}

void
Vehicle::Command_completed(
    ugcs::vsm::Vehicle_request::Result result,
    std::string status_text,
    Ucs_request::Ptr ucs_request)
{
    if (result == Vehicle_request::Result::OK) {
        ucs_request->Complete(ugcs::vsm::proto::STATUS_OK, status_text);
        if (ucs_request->response) {
            VEHICLE_LOG_INF((*this), "COMMAND OK");
        }
    } else {
        ucs_request->Complete(ugcs::vsm::proto::STATUS_FAILED, status_text);
        if (ucs_request->response) {
            VEHICLE_LOG_WRN((*this), "COMMAND FAILED: %s", status_text.c_str());
        }
    }
}

void
Vehicle::Handle_vehicle_request(Vehicle_task_request::Handle)
{
    LOG_DEBUG("Mission to vehicle [%s:%s] is ignored.", 
    Get_serial_number().c_str(), Get_model_name().c_str());
}

void
Vehicle::Handle_vehicle_request(Vehicle_command_request::Handle)
{
    LOG_DEBUG("Command for vehicle [%s:%s] is ignored.", Get_serial_number().c_str(),
            Get_model_name().c_str());
}

const std::string&
Vehicle::Get_serial_number() const
{
    return serial_number;
}

const std::string&
Vehicle::Get_model_name() const
{
    return model_name;
}

const std::string&
Vehicle::Get_autopilot_serial() const
{
    return autopilot_serial;
}

const std::string&
Vehicle::Get_autopilot_type() const
{
    return autopilot_type;
}

const std::string&
Vehicle::Get_port_name() const
{
    return port_name;
}
ugcs::vsm::mavlink::MAV_AUTOPILOT
Vehicle::Get_autopilot() const
{
    return autopilot;
}

Vehicle::Sys_status::Sys_status(
        bool uplink_connected,
        bool downlink_connected,
        Control_mode control_mode,
        State state,
        std::chrono::seconds uptime) :
        uplink_connected(uplink_connected), downlink_connected(downlink_connected),
        control_mode(control_mode), state(state), uptime(uptime)
{

}

bool
Vehicle::Sys_status::operator==(const Sys_status& other) const
{
    return uplink_connected == other.uplink_connected &&
           downlink_connected == other.downlink_connected &&
           state == other.state &&
           control_mode == other.control_mode;
}

void
Vehicle::Set_system_status(const Sys_status& system_status)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(system_status == this->sys_status);
        this->sys_status = system_status;
    }
    CREATE_COMMIT_SCOPE;
    if (update) {
        switch (this->sys_status.state) {
        case Sys_status::State::ARMED:
            t_is_armed->Set_value(true);
            break;
        case Sys_status::State::DISARMED:
            t_is_armed->Set_value(false);
            break;
        case Sys_status::State::UNKNOWN:
            t_is_armed->Set_value_na();
            break;
        }
        switch (this->sys_status.control_mode) {
        case Sys_status::Control_mode::AUTO:
            t_control_mode->Set_value(proto::CONTROL_MODE_AUTO);
            break;
        case Sys_status::Control_mode::MANUAL:
            t_control_mode->Set_value(proto::CONTROL_MODE_MANUAL);
            break;
        case Sys_status::Control_mode::GUIDED:
            t_control_mode->Set_value(proto::CONTROL_MODE_CLICK_GO);
            break;
        case Sys_status::Control_mode::JOYSTICK:
            t_control_mode->Set_value(proto::CONTROL_MODE_JOYSTICK);
            break;
        case Sys_status::Control_mode::UNKNOWN:
            t_control_mode->Set_value_na();
            break;
        }
        t_downlink_present->Set_value(this->sys_status.downlink_connected);
        t_uplink_present->Set_value(this->sys_status.uplink_connected);
    }
}

Vehicle::Sys_status
Vehicle::Get_system_status() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return sys_status;
}

Vehicle::Capabilities
Vehicle::Get_capabilities() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return capabilities;
}

void
Vehicle::Set_capabilities(const Capabilities& capabilities)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(this->capabilities == capabilities);
        this->capabilities = capabilities;
    }

    CREATE_COMMIT_SCOPE;

    if (update) {
        // legacy stuff. translate capability states to command states.

        c_mission_upload->Set_available();  // always available
#define SET_STATE(comma,state) \
        if (comma) {    \
            comma->Set_available(capabilities.Is_set(Capability::state));    \
        }

        SET_STATE(c_arm, ARM_AVAILABLE);
        SET_STATE(c_adsb_install, ADSB_TRANSPONDER_AVAILABLE);
        SET_STATE(c_adsb_operating, ADSB_TRANSPONDER_AVAILABLE);
        SET_STATE(c_adsb_preflight, ADSB_TRANSPONDER_AVAILABLE);
        SET_STATE(c_auto, AUTO_MODE_AVAILABLE);
        SET_STATE(c_disarm, DISARM_AVAILABLE);
        SET_STATE(c_waypoint, WAYPOINT_AVAILABLE);
        SET_STATE(c_guided, GUIDED_MODE_AVAILABLE);
        SET_STATE(c_manual, MANUAL_MODE_AVAILABLE);
        SET_STATE(c_pause, PAUSE_MISSION_AVAILABLE);
        SET_STATE(c_resume, RESUME_MISSION_AVAILABLE);
        SET_STATE(c_rth, RETURN_HOME_AVAILABLE);
        SET_STATE(c_land_command, LAND_AVAILABLE);
        SET_STATE(c_joystick, JOYSTICK_MODE_AVAILABLE);
        SET_STATE(c_direct_vehicle_control, DIRECT_VEHICLE_CONTROL_AVAILABLE);
        SET_STATE(c_takeoff_command, TAKEOFF_AVAILABLE);
        SET_STATE(c_emergency_land, EMERGENCY_LAND_AVAILABLE);
        SET_STATE(c_camera_trigger_command, CAMERA_TRIGGER_AVAILABLE);
        SET_STATE(c_direct_payload_control, DIRECT_PAYLOAD_CONTROL_AVAILABLE);
        SET_STATE(c_camera_power, PAYLOAD_POWER_AVAILABLE);
        SET_STATE(c_camera_video_source, SWITCH_VIDEO_SOURCE_AVAILABLE);
#undef SET_STATE
    }
}

Vehicle::Capability_states
Vehicle::Get_capability_states() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return capability_states;
}

void
Vehicle::Set_capability_states(const Capability_states& capability_states)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(this->capability_states == capability_states);
        this->capability_states = capability_states;
    }
    CREATE_COMMIT_SCOPE;
    if (update) {
        // legacy stuff. translate capability states to command states.
        c_mission_upload->Set_enabled(); // always enabled.

#define SET_STATE(comma,state) \
        if (comma) {    \
            comma->Set_enabled(capability_states.Is_set(Capability_state::state));    \
        }

        SET_STATE(c_arm, ARM_ENABLED);
        SET_STATE(c_adsb_install, ADSB_TRANSPONDER_ENABLED);
        SET_STATE(c_adsb_operating, ADSB_TRANSPONDER_ENABLED);
        SET_STATE(c_adsb_preflight, ADSB_TRANSPONDER_ENABLED);
        SET_STATE(c_auto, AUTO_MODE_ENABLED);
        SET_STATE(c_disarm, DISARM_ENABLED);
        SET_STATE(c_waypoint, WAYPOINT_ENABLED);
        SET_STATE(c_guided, GUIDED_MODE_ENABLED);
        SET_STATE(c_manual, MANUAL_MODE_ENABLED);
        SET_STATE(c_pause, PAUSE_MISSION_ENABLED);
        SET_STATE(c_resume, RESUME_MISSION_ENABLED);
        SET_STATE(c_rth, RETURN_HOME_ENABLED);
        SET_STATE(c_land_command, LAND_ENABLED);
        SET_STATE(c_joystick, JOYSTICK_MODE_ENABLED);
        SET_STATE(c_direct_vehicle_control, DIRECT_VEHICLE_CONTROL_ENABLED);
        SET_STATE(c_takeoff_command, TAKEOFF_ENABLED);
        SET_STATE(c_emergency_land, EMERGENCY_LAND_ENABLED);
        SET_STATE(c_camera_trigger_command, CAMERA_TRIGGER_ENABLED);
        SET_STATE(c_direct_payload_control, DIRECT_PAYLOAD_CONTROL_ENABLED);
        SET_STATE(c_camera_power, PAYLOAD_POWER_ENABLED);
        SET_STATE(c_camera_video_source, SWITCH_VIDEO_SOURCE_ENABLED);
#undef SET_STATE
    }
}

unsigned int
Vehicle::Add_subdevice(Subdevice_type type)
{
    subdevices.emplace_back(type);
    return subdevices.size() - 1;
}

Property::Ptr
Vehicle::Add_telemetry(
    unsigned int subdevice,
    const std::string& name,
    ugcs::vsm::proto::Field_semantic semantic,
    uint32_t timeout)
{
    auto t = Device::Add_telemetry(name, semantic, timeout);
    return Add_telemetry(subdevice, t);
}

Property::Ptr
Vehicle::Add_telemetry(
    unsigned int subdevice,
    const std::string& name,
    ugcs::vsm::Property::Value_type type,
    uint32_t timeout)
{
    auto t = Device::Add_telemetry(name, type, timeout);
    return Add_telemetry(subdevice, t);
}

Property::Ptr
Vehicle::Add_telemetry(
    unsigned int subdevice,
    Property::Ptr t)
{
    if (subdevice > subdevices.size()) {
        VSM_EXCEPTION(Exception, "Invalid subdevice %d for telemetry field %s", subdevice, t->Get_name().c_str());
    }
    auto &fields = subdevices[subdevice].telemetry_fields;
    if (fields.find(t->Get_name()) != fields.end()) {
        VSM_EXCEPTION(Exception, "Telemetry field %s already added", t->Get_name().c_str());
    }
    fields.emplace(t->Get_name(), t);
    return t;
}

void
Vehicle::Remove_telemetry(Property::Ptr& p)
{
    for (auto &d : subdevices) {
        for (auto f = d.telemetry_fields.begin(); f != d.telemetry_fields.end(); f++) {
            if (f->second->Get_id() == p->Get_id()) {
                d.telemetry_fields.erase(f);
                return;
            }
        }
    }
    Device::Remove_telemetry(p);
}

Vsm_command::Ptr
Vehicle::Add_command(
    unsigned int subdevice,
    const std::string& name,
    bool available_as_command,
    bool available_in_mission)
{
    if (subdevice > subdevices.size()) {
        VSM_EXCEPTION(Exception, "Invalid subdevice %d for command %s", subdevice, name.c_str());
    }
    auto &cmds = subdevices[subdevice].commands;
    if (cmds.find(name) != cmds.end()) {
        VSM_EXCEPTION(Exception, "Command %s already added", name.c_str());
    }
    auto c = Device::Add_command(name, available_as_command, available_in_mission);
    cmds.emplace(name, c);
    return c;
}

namespace {

uint64_t
fnv1a_hash64(const uint8_t *x, size_t n)
{
    uint64_t v = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) {
        v ^= x[i];
        v *= 0x100000001b3ULL;
    }
    return v;
}

} /* anonymous namespace */

/*
 * WARNING:
 * When changing this function, make sure to sync it with
 * VehicleIdentity::getVehicleId() in vsm-java-sdk.
 */
void
Vehicle::Calculate_system_id()
{
    if (serial_number.empty()) {
        VSM_EXCEPTION(Exception, "Serial number should be set before ID is calculated");
    }
    if (model_name.empty()) {
        VSM_EXCEPTION(Exception, "Model name should be set before ID is calculated");
    }
    auto seed = serial_number + model_name;
    const char* buff = seed.c_str();
    auto h = fnv1a_hash64(reinterpret_cast<const uint8_t*>(buff), seed.length());
    system_id = h;
    Add_property("system_id", system_id, Property::VALUE_TYPE_INT);
}

bool
Vehicle::Is_model_name_hardcoded()
{
    return model_name_is_hardcoded;
}

namespace {
void
Set_failsafe_actions(Property::Ptr p, std::initializer_list<proto::Failsafe_action> actions)
{
    for (auto a : actions) {
        switch (a) {
        case proto::FAILSAFE_ACTION_CONTINUE:
            p->Add_enum("continue", a);
            break;
        case proto::FAILSAFE_ACTION_WAIT:
            p->Add_enum("wait", a);
            break;
        case proto::FAILSAFE_ACTION_LAND:
            p->Add_enum("land", a);
            break;
        case proto::FAILSAFE_ACTION_RTH:
            p->Add_enum("rth", a);
            break;
        }
    }
    auto i = actions.begin();
    if (i != actions.end()) {
        p->Default_value()->Set_value(*i);
    }
}
}

void
Vehicle::Set_rc_loss_actions(std::initializer_list<proto::Failsafe_action> actions)
{
    Set_failsafe_actions(p_rc_loss_action, actions);
}

void
Vehicle::Set_low_battery_actions(std::initializer_list<proto::Failsafe_action> actions)
{
    Set_failsafe_actions(p_low_battery_action, actions);
}

void
Vehicle::Set_gps_loss_actions(std::initializer_list<proto::Failsafe_action> actions)
{
    Set_failsafe_actions(p_gps_loss_action, actions);
}
