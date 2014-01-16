// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file hello_world_vsm.cpp
 * "Hello world" VSM implementation example.
 */

/* Main include file for VSM SDK. Should be included in all VSM applications. */
/** [main include] */
#include <vsm/vsm.h>
/** [main include] */

/* Custom vehicle defined by VSM developer. */
/** [custom vehicle] */
class Custom_vehicle:public vsm::Vehicle
{
    /* To add shared pointer capability to this class. */
    DEFINE_COMMON_CLASS(Custom_vehicle, vsm::Vehicle)
/** [custom vehicle] */
public:
    /* Custom constructor. Vehicle and autopilot types are fixed, but serial
     * number and model name are passed to the constructor.
     */
    /** [vehicle constructor] */
    Custom_vehicle(
            const std::string& serial_number):
                vsm::Vehicle(
                        vsm::mavlink::MAV_TYPE::MAV_TYPE_GENERIC,
                        vsm::mavlink::MAV_AUTOPILOT::MAV_AUTOPILOT_GENERIC,
                        serial_number,
                        "MyDrone") {}
    /** [vehicle constructor] */

    /* Override enable method to perform initialization. In this example -
     * just start telemetry simulation timer.
     */
    /** [on enable] */
    virtual void
    On_enable() override
    {
        timer = vsm::Timer_processor::Get_instance()->Create_timer(
                /* Telemetry with 1 second granularity. */
                std::chrono::seconds(1),
                /* Send_telemetry method will be called on timer. Smart pointer
                 * is used to reference vehicle instance. Plain 'this' could also
                 * be used here, but it is generally not recommended. */
                /** [shared this for timer] */
                vsm::Make_callback(&Custom_vehicle::Send_telemetry, Shared_from_this()),
                /** [shared this for timer] */
                /* Execution will be done in default vehicle context,
                 * which is served by dedicated vehicle thread together with
                 * vehicle requests so additional synchronization is not necessary.
                 */
                Get_completion_ctx());

        Set_system_status(
                vsm::mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_SAFETY_ARMED,
                vsm::mavlink::MAV_STATE::MAV_STATE_ACTIVE,
                /* Uplink & downlink connected. */
                vsm::Vehicle::Custom_mode(true, true),
                /* Uptime is always 1 second, as an example only. */
                std::chrono::seconds(1));
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

    /* Handler for a task submitted for the vehicle by UgCS. */
    /** [task request] */
    virtual void
    Handle_vehicle_request(vsm::Vehicle_task_request::Handle request) override
    {
        /* Just print out all actions to the log. Here pointer access semantics
         * of the handle is used. */
        for (auto &action: request->actions) {
            LOG_DEBUG("Action %s received.", action->Get_name().c_str());
            /* Specific processing of move action. */
            if (action->Get_type() == vsm::Action::Type::MOVE) {
                /* Get move action class. */
                vsm::Move_action::Ptr move = action->Get_action<vsm::Action::Type::MOVE>();
                /* Get movement position as geodetic tuple. */
                vsm::Geodetic_tuple pos = move->position.Get_geodetic();
                /* And output the altitude only. */
                LOG_DEBUG("Move to altitude of %.2f meters.", pos.altitude);
            }
        }
        /* Indicate successful request processing by the vehicle. */
        request = vsm::Vehicle_request::Result::OK;
    }
    /** [task request] */

    /* Handler for a clear mission request submitted for the vehicle by UgCS. */
    /** [clear all missions] */
    virtual void
    Handle_vehicle_request(vsm::Vehicle_clear_all_missions_request::Handle request) override
    {
        /* Mimic operation execution. */
        LOG_DEBUG("All missions cleared.");
        /* Indicate successful request processing by the vehicle. */
        request = vsm::Vehicle_request::Result::OK;
    }
    /** [clear all missions] */


    /* Handler for a command submitted for the vehicle by UgCS. */
    /** [command request] */
    virtual void
    Handle_vehicle_request(vsm::Vehicle_command_request::Handle request) override
    {
        /* Mimic command execution. Here dereference access semantics of the
         * handle is used. */
        switch ((*request).Get_type()) {
        case vsm::Vehicle_command::Type::GO:
            LOG_DEBUG("Vehicle launched!");
            /* Start yaw spinning and climbing. */
            yaw_speed = 0.1;
            climb_speed = 0.5;
            break;
        case vsm::Vehicle_command::Type::HOLD:
            LOG_DEBUG("Vehicle paused.");
            /* Stop yaw spinning and climbing. */
            yaw_speed = 0;
            climb_speed = 0;
            break;
        }
        /* Indicate successful request processing by the vehicle. */
        request = vsm::Vehicle_request::Result::OK;
    }
    /** [command request] */

private:

    /* Generate and send simulated telemetry data to UgCS. */
    /** [telemetry] */
    bool
    Send_telemetry()
    {
        auto report = Open_telemetry_report();
        report->Set<vsm::telemetry::Yaw>(yaw);
        /* Simulate some spinning between [-Pi;+Pi]. */
        if ((yaw += yaw_speed) >= M_PI) {
            yaw = -M_PI;
        }

        /* Simulate climbing high to the sky. */
        altitude += climb_speed;
        report->Set<vsm::telemetry::Abs_altitude>(altitude);

        /* Report also some battery value. */
        report->Set<vsm::telemetry::Battery_voltage>(13.5);

        /* Return true to reschedule the same timer again. */
        return true;
    }
    /** [telemetry] */

    /** Timer for telemetry simulation. */
    vsm::Timer_processor::Timer::Ptr timer;

    /** Current yaw value. */
    double yaw = 0;

    /** Current yaw speed in radians/sec. */
    double yaw_speed = 0;

    /** Current altitude. */
    double altitude = 0;

    /** Climb speed. */
    double climb_speed = 0;
};

int main(int, char* [])
{
    /** [init] */
    /* First of all, initialize SDK infrastructure and services. */
    vsm::Initialize();
    /** [init] */
    /** [instance creation] */
    /** [instance creation and enable] */
    /* Create vehicle instance with hard-coded serial number. */
    Custom_vehicle::Ptr vehicle = Custom_vehicle::Create("123456");
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
    vsm::Terminate();
    /** [exit sequence] */
    return 0;
}
