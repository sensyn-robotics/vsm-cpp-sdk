// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle.h
 *
 * Vehicle interface representation.
 */

#ifndef VEHICLE_H_
#define VEHICLE_H_

#include <vsm/request_worker.h>
#include <vsm/vehicle_requests.h>
#include <vsm/telemetry_manager.h>
#include <vsm/mavlink.h>

#include <stdint.h>
#include <memory>

namespace vsm {

/** Base class for user-defined vehicles. It contains interface to SDK services
 * which can be used as base class methods calls, and abstract interface which
 * must be implemented by the device.
 * Instance creation should be done by {@link Vehicle::Create()} method.
 */
class Vehicle: public std::enable_shared_from_this<Vehicle> {
    DEFINE_COMMON_CLASS(Vehicle, Vehicle)
public:
    /**
     * Constructor for a base class of user defined vehicle instance. Vehicle
     * instance is identified by serial number and model name which combination
     * should be unique in the whole UgCS domain (i.e. all interconnected
     * UCS and VSM systems).
     * @param type Type of the vehicle.
     * @param autopilot Autopilot type of the vehicle.
     * @param serial_number Serial number of the vehicle.
     * @param model_name Model name of the vehicle.
     * @param create_thread If @a true, then separate thread is automatically
     * created for vehicle instance which allows to using blocking methods
     * in the context of this vehicle without blocking other vehicles, otherwise
     * vehicle instance thread is not created and user is supposed to call
     * @ref Vehicle::Process_requests method to process requests pending for
     * this vehicle. It is recommended to leave this value as default, i.e.
     * "true".
     */
    Vehicle(mavlink::MAV_TYPE type, mavlink::MAV_AUTOPILOT autopilot,
            const std::string& serial_number = std::string(),
            const std::string& model_name = std::string(),
    		bool create_thread = true);

    /** Enable the instance. Should be called right after vehicle instance
     * creation.
     */
    void
    Enable();

    /** Disable the instance. Should be called prior to intention to delete
     * the instance.
     */
    void
    Disable();

    /** Vehicle enable/disable status. */
    bool
    Is_enabled();

    /** Make sure class is polymorphic. */
    virtual
    ~Vehicle() {};

    /** Disable copying. */
    Vehicle(const Vehicle &) = delete;

    /** Process requests assigned to vehicle in user thread, i.e. the thread
     * which calls this method. Supposed to be called only when vehicle is
     * created without its own thread.
     *
     */
    void
    Process_requests();

    /** Get serial number of the vehicle. */
    const std::string&
    Get_serial_number() const;

    /** Get model name of the vehicle */
    const std::string&
    Get_model_name() const;

    /** Get default completion context of the vehicle. */
    Request_completion_context::Ptr
    Get_completion_ctx();

    /** Hasher for Vehicle shared pointer. Used when vehicle pointer is
     * stored in some container. */
    class Hasher
    {
    public:
        /** Get hash value. */
        size_t
        operator ()(const Vehicle::Ptr& ptr) const
        {
            return hasher(ptr.get());
        }
    private:
        /** Standard pointer hasher. */
        static std::hash<Vehicle*> hasher;
    };

protected:
    /** Serial number of the vehicle. */
    const std::string serial_number;

    /** Model of the vehicle, e.g. Arducopter, Mikrokopter etc. */
    const std::string model_name;

    /** Type of the vehicle. */
    const mavlink::MAV_TYPE type;

    /** Get default processing context of the vehicle. */
    Request_processor::Ptr
    Get_processing_ctx();

    /** Vehicle enable event handler. Can be overridden by derived class,
     * if necessary.
     */
    virtual void
    On_enable()
    {};

    /** Vehicle disable event handler. Can be overridden by derived class,
     * if necessary.
     */
    virtual void
    On_disable()
    {};

    /*
     * Below are methods which are called by VSM SDK in vehicle context and
     * should be overridden by user code.
     */

    /**
     * Task has arrived from UCS and should be uploaded to the vehicle.
     */
    virtual void
    Handle_vehicle_request(Vehicle_task_request::Handle request);

    /**
     * UCS requesting to clear up all the missions on a vehicle.
     */
    virtual void
    Handle_vehicle_request(Vehicle_clear_all_missions_request::Handle request);

    /**
     * UCS sending a command to the vehicle.
     */
    virtual void
    Handle_vehicle_request(Vehicle_command_request::Handle request);

    /* End of methods called by VSM SDK. */

    /* Below are methods which should be called by user code from derived
     * vehicle class. */

    /** Custom mode of the vehicle. */
    class Custom_mode {
    public:
        /** Construct custom mode. */
        Custom_mode(bool uplink_connected, bool downlink_connected);

        /** State of the uplink connection to the vehicle. */
        bool uplink_connected;

        /** State of the downlink connection from the vehicle. */
        bool downlink_connected;
    };

    /** Set system status of this vehicle. Should be called when system
     * status changes, but at least with reasonable granularity to update
     * system uptime.
     */
    void
    Set_system_status(mavlink::MAV_MODE_FLAG system_mode,
                      mavlink::MAV_STATE system_state,
                      const Custom_mode& custom_mode,
                      std::chrono::seconds uptime);

    /** Open context for telemetry reporting. */
    Telemetry_manager::Report::Ptr
    Open_telemetry_report();

    /* End of user callable methods. */

private:

    /** Register vehicle instance to UCS processor. After registration is done,
     * UCS servers sees that vehicle is available as such.
     */
    void
    Register();

    /** Unregister vehicle instance from UCS processor. */
    void
    Unregister();

    /** Calculate system id for this Vehicle. */
    void
    Calculate_system_id();

    /** Submit vehicle request to be processed by this vehicle. Conversion to
     * appropriate user handle type is automatic.
     */
    template<class Request_ptr>
    void
    Submit_vehicle_request(Request_ptr vehicle_request)
    {
        using Request = typename Request_ptr::element_type;
        using Handle = typename Request::Handle;
        typedef void (Vehicle::*Handler)(Handle);

        Handler method = &Vehicle::Handle_vehicle_request;
        auto proc_handler = Make_callback(method,
                                          Shared_from_this(),
                                          Handle(vehicle_request));
        vehicle_request->request->Set_processing_handler(proc_handler);
        processor->Submit_request(vehicle_request->request);
    }

    /** Register new telemetry interface towards UCS server. */
    void
    Register_telemetry_interface(Telemetry_interface& telemetry);

    /** Unregister existing telemetry interface towards UCS server. */
    void
    Unregister_telemetry_interface();

    /** Type of the autopilot. */
    const mavlink::MAV_AUTOPILOT autopilot;

    /** System mode of the vehicle. */
    mavlink::MAV_MODE_FLAG system_mode = static_cast<mavlink::MAV_MODE_FLAG>(0);

    /** System state of the vehicle. */
    mavlink::MAV_STATE system_state = mavlink::MAV_STATE::MAV_STATE_UNINIT;

    /** Custom mode of the vehicle. */
    Custom_mode custom_mode = Custom_mode(false, false);

    /** System id of this vehicle as seen by UCS server. */
    mavlink::System_id system_id;

    /** Vehicle uptime. */
    std::chrono::seconds uptime;

    /** Is vehicle enabled. */
    bool is_enabled = false;

    Request_completion_context::Ptr completion_ctx;
    Request_processor::Ptr processor;
    Request_worker::Ptr worker;

    Telemetry_manager telemetry;

    /* Friend classes mostly for accessing system_id variable which we want
     * to hide from SDK user.
     */
    friend class Ucs_vehicle_ctx;
    friend class Ucs_transaction;
    friend class Ucs_mission_clear_all_transaction;
    friend class Ucs_task_upload_transaction;
    friend class Ucs_vehicle_command_transaction;
    friend class Cucs_processor;
};

} /* namespace vsm */

#endif /* VEHICLE_H_ */
