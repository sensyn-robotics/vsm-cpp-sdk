// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file hello_world_vsm.cpp
 * "Hello world" VSM implementation example.
 */

/* Main include file for VSM SDK. Should be included in all VSM applications. */
/** [main include] */
#include <ugcs/vsm/vsm.h>
/** [main include] */

/* Custom vehicle defined by VSM developer. */
/** [custom vehicle] */
class Custom_vehicle:public ugcs::vsm::Device
{
    /* To add shared pointer capability to this class. */
    DEFINE_COMMON_CLASS(Custom_vehicle, ugcs::vsm::Vehicle)
/** [custom vehicle] */
public:
    /* Custom constructor. Vehicle, autopilot and capabilities are fixed,
     * but serial number and model name are passed to the constructor.
     */
    /** [vehicle constructor] */
    Custom_vehicle(const std::string& serial_number): serial_number(serial_number)
    {
    /** [vehicle constructor] */
        /* Create telemetry fields
         * The exact type of the field is derived from name. */
        t_control_mode = Add_telemetry("control_mode");
        t_main_voltage = Add_telemetry("main_voltage");
        t_heading = Add_telemetry("heading");
        t_altitude_raw = Add_telemetry("altitude_raw");

        /* For telemetry fields without hardcoded type it can be specified explicitly. */
        t_uplink_present = Add_telemetry("uplink_present", ugcs::vsm::proto::FIELD_SEMANTIC_BOOL);
        t_is_armed = Add_telemetry("is_armed", ugcs::vsm::proto::FIELD_SEMANTIC_BOOL);
        t_downlink_present = Add_telemetry("downlink_present", ugcs::vsm::Property::VALUE_TYPE_BOOL);

        /* Create commands which are available as standalone commands */
        c_mission_upload = Add_command("mission_upload", true, false);
        c_arm = Add_command("arm", true, false);
        c_disarm = Add_command("disarm", true, false);

        /* Create commands which can be issued as part of mission */
        c_move = Add_command("move", false, true);
        c_move->Add_parameter("latitude");
        c_move->Add_parameter("longitude");
        c_move->Add_parameter("altitude_amsl");
        c_move->Add_parameter("acceptance_radius");
        c_move->Add_parameter("loiter_radius", ugcs::vsm::Property::VALUE_TYPE_FLOAT);
        c_move->Add_parameter("wait_time", ugcs::vsm::Property::VALUE_TYPE_FLOAT);
        c_move->Add_parameter("heading");
        c_move->Add_parameter("ground_elevation");
    }

    virtual void
    Fill_register_msg(ugcs::vsm::proto::Vsm_message& msg)
    {
        auto dev = msg.mutable_register_device();

        // Set up the begin of epoch. This is required to maintain correct timestamps for telemetry.
        dev->set_begin_of_epoch(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                begin_of_epoch.time_since_epoch()).count());

        auto reg = dev->mutable_register_vehicle();

        // Set vehicle specific properties.
        reg->set_vehicle_type(ugcs::vsm::proto::VEHICLE_TYPE_MULTICOPTER);
        reg->set_vehicle_serial(serial_number);
        reg->set_port_name("simulation");

        // Register "flight controller" subsystem.
        auto sd = reg->add_register_flight_controller();

        // autopilot_type is mandatory.
        sd->set_autopilot_type("my_t");

        // Associate telemetry fields with flight controller.
        t_control_mode->Register(sd->add_telemetry_fields());
        t_main_voltage->Register(sd->add_telemetry_fields());
        t_heading->Register(sd->add_telemetry_fields());
        t_altitude_raw->Register(sd->add_telemetry_fields());
        t_is_armed->Register(sd->add_telemetry_fields());
        t_uplink_present->Register(sd->add_telemetry_fields());
        t_downlink_present->Register(sd->add_telemetry_fields());

        // Associate commands with flight controller.
        c_mission_upload->Register(sd->add_commands());
        c_disarm->Register(sd->add_commands());
        c_arm->Register(sd->add_commands());
        c_move->Register(sd->add_commands());
    }

    /* Override enable method to perform initialization. In this example -
     * just start telemetry simulation timer.
     */
    /** [on enable] */
    virtual void
    On_enable() override
    {
        timer = ugcs::vsm::Timer_processor::Get_instance()->Create_timer(
                /* Telemetry with 1 second granularity. */
                std::chrono::seconds(1),
                /* Send_telemetry method will be called on timer. Smart pointer
                 * is used to reference vehicle instance. Plain 'this' could also
                 * be used here, but it is generally not recommended. */
                /** [shared this for timer] */
                ugcs::vsm::Make_callback(&Custom_vehicle::Send_telemetry, Shared_from_this()),
                /** [shared this for timer] */
                /* Execution will be done in default vehicle context,
                 * which is served by dedicated vehicle thread together with
                 * vehicle requests so additional synchronization is not necessary.
                 */
                Get_completion_ctx());

        /* Report current control mode to the UgCS */
        t_control_mode->Set_value(ugcs::vsm::proto::CONTROL_MODE_MANUAL);

        /* Report link state to the UgCS */
        t_downlink_present->Set_value(true);
        t_uplink_present->Set_value(true);

        /* Tell UgCS about available commands.
         * Command availability can be changed during runtime via this call and
         * VSM can determine which command buttons to show in the client UI. */
        c_arm->Set_available();
        c_disarm->Set_available();
        c_mission_upload->Set_available();

        /* Mission upload is always enabled.
        * Command state (enabled/disabled) can be changed during runtime via this call and
        * VSM can determine which command buttons to show as enabled in the client UI. */
        c_mission_upload->Set_enabled();
        Commit_to_ucs();
    }
    /** [on enable] */

    /** [on disable] */
    virtual void
    On_disable() override
    {
        /* Turn off the timer. */
        timer->Cancel();
    }
    /** [on disable] */

    /* Handler for a command submitted for the vehicle by UgCS. */
    /** [command request] */
    void
    Handle_ucs_command(ugcs::vsm::Ucs_request::Ptr ucs_request)
    {
        for (int c = 0; c < ucs_request->request.device_commands_size(); c++) {
            auto &vsm_cmd = ucs_request->request.device_commands(c);

            auto cmd = Get_command(vsm_cmd.command_id());

            LOG("COMMAND %s (%d) received",
                cmd->Get_name().c_str(),
                vsm_cmd.command_id());

            ugcs::vsm::Property_list params;

            try {
                if        (cmd == c_arm) {

                    LOG_DEBUG("Vehicle armed!");
                    /* Start yaw spinning and climbing. */
                    yaw_speed = 0.1;
                    climb_speed = 0.5;

                    /* Simulate armed vehicle */
                    is_armed = true;

                } else if (cmd == c_disarm) {

                    LOG_DEBUG("Vehicle disarmed.");
                    /* Stop yaw spinning and climbing. */
                    yaw_speed = 0;
                    climb_speed = 0;

                    /* Simulate disarmed vehicle */
                    is_armed = false;

                } else if (cmd == c_disarm) {

                    LOG_DEBUG("Vehicle disarmed.");
                    /* Stop yaw spinning and climbing. */
                    yaw_speed = 0;
                    climb_speed = 0;

                    /* Simulate disarmed vehicle */
                    is_armed = false;

                } else if (cmd == c_mission_upload) {

                    /* Iterate over all mission commands */
                    for (int item_count = 0; item_count < vsm_cmd.sub_commands_size(); item_count++) {
                        auto vsm_scmd = vsm_cmd.sub_commands(item_count);
                        auto cmd = Get_command(vsm_scmd.command_id());
                        if (cmd == c_move) {
                            /* Only move is supported by this vehicle */
                            LOG("MISSION item %d %s (%d)",
                                item_count, cmd->Get_name().c_str(),
                                vsm_scmd.command_id());
                            params = cmd->Build_parameter_list(vsm_scmd);
                            float alt;
                            params.find("altitude")->second->Get_value(alt);
                            LOG_DEBUG("Move to altitude of %.2f meters.", alt);
                        }
                    }

                } else {
                    VSM_EXCEPTION(ugcs::vsm::Action::Format_exception, "Unsupported command");
                }
                ucs_request->Complete();
            } catch (const std::exception& ex) {
                ucs_request->Complete(ugcs::vsm::proto::STATUS_INVALID_PARAMETER, std::string(ex.what()));
            }
        }
    }
    /** [command request] */

private:

    /* Generate and send simulated telemetry data to UgCS. */
    /** [telemetry] */
    bool
    Send_telemetry()
    {

        /* Report current heading. */
        t_heading->Set_value(yaw);

        /* Simulate some spinning between [-Pi;+Pi]. */
        if ((yaw += yaw_speed) >= M_PI) {
            yaw = -M_PI;
        }

        /* Simulate climbing high to the sky. */
        altitude += climb_speed;
        t_altitude_raw->Set_value(altitude);

        /* Report also some battery value. */
        t_main_voltage->Set_value(13.5);

        /* Enable ARM command if vehicle is disarmed. */
        c_arm->Set_enabled(!is_armed);

        /* Enable DISARM command if vehicle is armed. */
        c_disarm->Set_enabled(is_armed);

        /* Report armed state to the UgCS */
        t_is_armed->Set_value(is_armed);

        /* Send the updated telemetry fields and command states to UgCS
         * SDK will send only modified values thus saving bandwidth */
        Commit_to_ucs();

        /* Return true to reschedule the same timer again. */
        return true;
    }
    /** [telemetry] */

    std::string serial_number;
    /** Timer for telemetry simulation. */
    ugcs::vsm::Timer_processor::Timer::Ptr timer;

    /** Current yaw value. */
    double yaw = 0;

    /** Current yaw speed in radians/sec. */
    double yaw_speed = 0;

    /** Current altitude. */
    double altitude = 0;

    /** Climb speed. */
    double climb_speed = 0;

    bool is_armed = false;

    /* Define supported telemetry fields */
    ugcs::vsm::Property::Ptr t_control_mode = nullptr;
    ugcs::vsm::Property::Ptr t_is_armed = nullptr;
    ugcs::vsm::Property::Ptr t_uplink_present = nullptr;
    ugcs::vsm::Property::Ptr t_downlink_present = nullptr;
    ugcs::vsm::Property::Ptr t_main_voltage = nullptr;
    ugcs::vsm::Property::Ptr t_heading = nullptr;
    ugcs::vsm::Property::Ptr t_altitude_raw = nullptr;

    /* Define supported commands */
    ugcs::vsm::Vsm_command::Ptr c_mission_upload = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_move = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_arm = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_disarm = nullptr;
};

int main(int, char* [])
{
    /** [init] */
    /* First of all, initialize SDK infrastructure and services. */
    ugcs::vsm::Initialize();
    /** [init] */
    /** [instance creation] */
    /** [instance creation and enable] */
    /* Create vehicle instance with hard-coded serial number. */
    Custom_vehicle::Ptr vehicle = Custom_vehicle::Create("asd123456");
    /** [instance creation] */
    /* Should be always called right after vehicle instance creation. */
    vehicle->Enable();
    /** [instance creation and enable] */
    /** [main loop] */
    /* Now vehicle is visible to UgCS. Requests for the vehicle will be executed
     * in a dedicated vehicle thread. In a real VSM, there should be some
     * supervision logic which monitors the connection with a vehicle and
     * disables it when connection is totally lost. Here it is assumed that
     * vehicle is always connected, so just wait indefinitely.
     */
    std::condition_variable cond;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock); /* Infinite wait. */
    /** [main loop] */
    /** [exit sequence] */
    /** [disable vehicle] */
    /* Disable the vehicle, considered to be deleted after that. */
    vehicle->Disable();
    /** [disable vehicle] */
    /* Gracefully terminate SDK infrastructure and services. */
    ugcs::vsm::Terminate();
    /** [exit sequence] */
    return 0;
}
