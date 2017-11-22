// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle.h
 *
 * Vehicle interface representation.
 */

#ifndef VEHICLE_H_
#define VEHICLE_H_

#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/vehicle_requests.h>
#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/enum_set.h>
#include <ugcs/vsm/device.h>
#include <ugcs/vsm/crc32.h>

#include <stdint.h>
#include <memory>
#include <unordered_map>

namespace ugcs {
namespace vsm {

static const std::string legacy_commands[] = {std::string("")};

static constexpr int DEFAULT_COMMAND_TRY_COUNT = 3;
static constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT(1000);

/** Base class for user-defined vehicles. It contains interface to SDK services
 * which can be used as base class methods calls, and abstract interface which
 * must be implemented by the device.
 * Instance creation should be done by {@link Vehicle::Create()} method.
 */
class Vehicle: public Device {
    DEFINE_COMMON_CLASS(Vehicle, Device)
public:

    // @{
    /** Capability of the vehicle.
    * This is legacy enum. do not add new commands here. */
    enum class Capability {
        ARM_AVAILABLE,
        DISARM_AVAILABLE,
        AUTO_MODE_AVAILABLE,
        MANUAL_MODE_AVAILABLE,
        RETURN_HOME_AVAILABLE,
        TAKEOFF_AVAILABLE,
        LAND_AVAILABLE,
        EMERGENCY_LAND_AVAILABLE,
        CAMERA_TRIGGER_AVAILABLE,
        WAYPOINT_AVAILABLE,
        PAUSE_MISSION_AVAILABLE,
        RESUME_MISSION_AVAILABLE,
        GUIDED_MODE_AVAILABLE,
        JOYSTICK_MODE_AVAILABLE,
        PAYLOAD_POWER_AVAILABLE,
        SWITCH_VIDEO_SOURCE_AVAILABLE,
        DIRECT_VEHICLE_CONTROL_AVAILABLE,
        DIRECT_PAYLOAD_CONTROL_AVAILABLE,
        LAST
    };
    // @}

    // @{
    /** State of the vehicle capability.
     * This is legacy enum. do not add new commands here. */
    enum class Capability_state {
        ARM_ENABLED,
        DISARM_ENABLED,
        AUTO_MODE_ENABLED,
        MANUAL_MODE_ENABLED,
        RETURN_HOME_ENABLED,
        TAKEOFF_ENABLED,
        LAND_ENABLED,
        EMERGENCY_LAND_ENABLED,
        CAMERA_TRIGGER_ENABLED,
        WAYPOINT_ENABLED,
        PAUSE_MISSION_ENABLED,
        RESUME_MISSION_ENABLED,
        GUIDED_MODE_ENABLED,
        JOYSTICK_MODE_ENABLED,
        PAYLOAD_POWER_ENABLED,
        SWITCH_VIDEO_SOURCE_ENABLED,
        DIRECT_VEHICLE_CONTROL_ENABLED,
        DIRECT_PAYLOAD_CONTROL_ENABLED,
        LAST
    };
    // @}

    /** Container type for storing vehicle capabilities. */
    typedef Enum_set<Capability> Capabilities;

    /** Container type for storing vehicle capability states. */
    typedef Enum_set<Capability_state> Capability_states;

    /**
     * Constructor for a base class of user defined vehicle instance. Vehicle
     * instance is identified by serial number and model name which combination
     * should be unique in the whole UgCS domain (i.e. all interconnected
     * UCS and VSM systems).
     * @param type Type of the vehicle.
     * @param autopilot Autopilot type of the vehicle.
     * @param capabilities Capabilities of the vehicle.
     * @param serial_number Serial number of the vehicle.
     * @param model_name Model name of the vehicle.
     * @param model_name_is_hardcoded tell the UCS that vehicle name is not
     *        specified by user or taken from the autopilot but
     *        hardcoded in VSM code instead.
     * @param create_thread If @a true, then separate thread is automatically
     * created for vehicle instance which allows to using blocking methods
     * in the context of this vehicle without blocking other vehicles, otherwise
     * vehicle instance thread is not created and user is supposed to call
     * @ref Vehicle::Process_requests method to process requests pending for
     * this vehicle. It is recommended to leave this value as default, i.e.
     * "true".
     */
    Vehicle(mavlink::MAV_TYPE type, mavlink::MAV_AUTOPILOT autopilot,
            const Capabilities& capabilities,
            const std::string& serial_number,
            const std::string& model_name,
            int model_name_is_hardcoded = false,
    		bool create_thread = true);

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

    /** Get port name the vehicle is connected to. */
    const std::string&
    Get_port_name() const;

    /** Get autopilot serial number */
    const std::string&
    Get_autopilot_serial() const;

    ugcs::vsm::mavlink::MAV_AUTOPILOT
    Get_autopilot() const;

    const std::string&
    Get_autopilot_type() const;

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

    typedef enum {
        SUBDEVICE_TYPE_FC = 1,
        SUBDEVICE_TYPE_CAMERA = 2,
        SUBDEVICE_TYPE_GIMBAL = 3,
        SUBDEVICE_TYPE_ADSB_TRANSPONDER = 4,
    } Subdevice_type;

    class Subdevice {
    public:
        Subdevice(Subdevice_type t):type(t){};
        Subdevice_type type;
        // Subdevice telemetry fields.
        std::unordered_map<std::string, Property::Ptr> telemetry_fields;
        // Commands supported by subdevice.
        std::unordered_map<std::string, Vsm_command::Ptr> commands;
    };

    /** Serial number of the vehicle. */
    const std::string serial_number;

    /** Model of the vehicle, e.g. Arducopter, Mikrokopter etc. */
    const std::string model_name;

    /** Port name the vehicle is connected to. ("COM12" or "127.0.0.1:5440")*/
    std::string port_name;

    /** Serial number of the autopilot. (empty if not available)*/
    std::string autopilot_serial;

    /** autopilot name */
    std::string autopilot_type;

    /** Frame type */
    std::string frame_type;

    /** Type of the vehicle. */
    const mavlink::MAV_TYPE type;   // Deprecated
    ugcs::vsm::proto::Vehicle_type vehicle_type;

    std::vector<Subdevice> subdevices;

    /** vehicle subsystems */
    typedef struct {
        unsigned int fc;
        unsigned int camera;
        unsigned int gimbal;
        unsigned int adsb_transponder;
    } Subsystems;

    Subsystems subsystems;

    ugcs::vsm::Optional<ugcs::vsm::proto::Flight_mode> current_flight_mode;

    ugcs::vsm::Property::Ptr t_control_mode = nullptr;
    ugcs::vsm::Property::Ptr t_is_armed = nullptr;
    ugcs::vsm::Property::Ptr t_uplink_present = nullptr;
    ugcs::vsm::Property::Ptr t_downlink_present = nullptr;
    ugcs::vsm::Property::Ptr t_main_voltage = nullptr;
    ugcs::vsm::Property::Ptr t_main_current = nullptr;
    ugcs::vsm::Property::Ptr t_gcs_link_quality = nullptr;
    ugcs::vsm::Property::Ptr t_rc_link_quality = nullptr;
    ugcs::vsm::Property::Ptr t_latitude = nullptr;
    ugcs::vsm::Property::Ptr t_longitude = nullptr;
    ugcs::vsm::Property::Ptr t_altitude_raw = nullptr;
    ugcs::vsm::Property::Ptr t_altitude_amsl = nullptr;
    ugcs::vsm::Property::Ptr t_ground_speed = nullptr;
    ugcs::vsm::Property::Ptr t_air_speed = nullptr;
    ugcs::vsm::Property::Ptr t_course = nullptr;
    ugcs::vsm::Property::Ptr t_vertical_speed = nullptr;
    ugcs::vsm::Property::Ptr t_pitch = nullptr;
    ugcs::vsm::Property::Ptr t_roll = nullptr;
    ugcs::vsm::Property::Ptr t_heading = nullptr;
    ugcs::vsm::Property::Ptr t_gps_fix = nullptr;
    ugcs::vsm::Property::Ptr t_satellite_count = nullptr;
    ugcs::vsm::Property::Ptr t_altitude_origin = nullptr;
    ugcs::vsm::Property::Ptr t_home_altitude_amsl = nullptr;
    ugcs::vsm::Property::Ptr t_home_altitude_raw = nullptr;
    ugcs::vsm::Property::Ptr t_home_latitude = nullptr;
    ugcs::vsm::Property::Ptr t_home_longitude = nullptr;
    ugcs::vsm::Property::Ptr t_target_altitude_amsl = nullptr;
    ugcs::vsm::Property::Ptr t_target_altitude_raw = nullptr;
    ugcs::vsm::Property::Ptr t_target_latitude = nullptr;
    ugcs::vsm::Property::Ptr t_target_longitude = nullptr;
    ugcs::vsm::Property::Ptr t_current_command = nullptr;
    ugcs::vsm::Property::Ptr t_current_mission_id = nullptr;
    ugcs::vsm::Property::Ptr t_flight_mode = nullptr;
    ugcs::vsm::Property::Ptr t_autopilot_status = nullptr;
    ugcs::vsm::Property::Ptr t_native_flight_mode = nullptr;
    ugcs::vsm::Property::Ptr t_fence_enabled = nullptr;

    ugcs::vsm::Vsm_command::Ptr c_mission_upload = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_auto = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_arm = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_disarm = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_waypoint = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_guided = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_manual = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_pause = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_resume = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_rth = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_land_command = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_joystick = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_direct_vehicle_control = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_takeoff_command = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_emergency_land = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_trigger_command = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_adsb_set_ident = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_adsb_set_mode = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_direct_payload_control = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_power = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_video_source = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_adsb_set_parameter = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_servo = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_repeat_servo = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_fence = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_trigger_calibration = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_trigger_reboot = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_get_native_route = nullptr;

    ugcs::vsm::Vsm_command::Ptr c_wait = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_move = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_speed = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_home = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_poi = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_heading = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_panorama = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_trigger_mission = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_by_time = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_camera_by_distance = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_land_mission = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_takeoff_mission = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_set_parameter = nullptr;
    ugcs::vsm::Vsm_command::Ptr c_payload_control = nullptr;

    Property::Ptr p_rc_loss_action = nullptr;
    Property::Ptr p_gps_loss_action = nullptr;
    Property::Ptr p_low_battery_action = nullptr;
    Property::Ptr p_wp_turn_type = nullptr;

    int command_try_count = DEFAULT_COMMAND_TRY_COUNT;
    std::chrono::milliseconds command_timeout = DEFAULT_COMMAND_TIMEOUT;

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

    void
    Set_rc_loss_actions(std::initializer_list<proto::Failsafe_action> actions);

    void
    Set_gps_loss_actions(std::initializer_list<proto::Failsafe_action> actions);

    void
    Set_low_battery_actions(std::initializer_list<proto::Failsafe_action> actions);

    /** Tell server that current altitude origin must be dropped.
     * (calibration needed to match reported altitude to real world)
     *
     * Use this when VSM knows that currently reported Rel_altitude
     * changed unexpectedly. For example if vehicle resets the reported
     * altitude on ARM. */
    void
    Reset_altitude_origin();

    /** Tell server that Vehicle knows its altitude_origin.
     * Use this when VSM knows that reported Rel_altitude origin
     * has changed. Ardupilot does that on home location change. */
    void
    Set_altitude_origin(float altitude_amsl);

    /*
     * Below are methods which are called by VSM SDK in vehicle context and
     * should be overridden by user code.
     */

    /**
     * Message from ucs arrived
     */

    // Translates incoming message to old style Vehicle_request
    virtual void
    Handle_ucs_command(
        Ucs_request::Ptr ucs_request);

    // old style completion handler
    void
    Command_completed(
        ugcs::vsm::Vehicle_request::Result result,
        const std::string& status_text,
        Ucs_request::Ptr ucs_request);

    void
    Command_failed(
        Ucs_request::Ptr ucs_request,
        const std::string& status_text,
        proto::Status_code code = ugcs::vsm::proto::STATUS_FAILED);

    void
    Command_succeeded(
        Ucs_request::Ptr ucs_request);

    /**
     * Task has arrived from UCS and should be uploaded to the vehicle.
     */
    virtual void
    Handle_vehicle_request(Vehicle_task_request::Handle request);

    /**
     * UCS sending a command to the vehicle.
     */
    virtual void
    Handle_vehicle_request(Vehicle_command_request::Handle request);

    // Used by Cucs_processor only.
    // Derived class must override.
    virtual void
    Fill_register_msg(ugcs::vsm::proto::Vsm_message&);

    /* End of methods called by VSM SDK. */

    /* Below are methods which should be called by user code from derived
     * vehicle class. */

    /** System status of the vehicle. */
    class Sys_status {
    public:

        /** Control mode of the vehicle. */
        enum class Control_mode {
            /** Direct manual control via RC transmitter */
            MANUAL,

            /** Automatic control. */
            AUTO,

            /** Manual control via single WP. */
            GUIDED,

            /** Direct manual control via joystick. */
            JOYSTICK,

            /** Unknown method of control. */
            UNKNOWN
        };

        /** State of the vehicle. */
        enum class State {
            /** Vehicle is disarmed. */
            DISARMED,

            /** Vehicle is armed. */
            ARMED,

            /** Vehicle state is unknown. */
            UNKNOWN
        };

        /** Construct system status. */
        Sys_status(
                bool uplink_connected,
                bool downlink_connected,
                Control_mode control_mode,
                State state,
                std::chrono::seconds uptime);

        /** Equality operator. */
        bool
        operator==(const Sys_status&) const;

        /** State of the uplink connection to the vehicle. */
        bool uplink_connected;

        /** State of the downlink connection from the vehicle. */
        bool downlink_connected;

        /** Current control mode. */
        Control_mode control_mode;

        /** Current state of the vehicle. */
        State state;

        /** Vehicle uptime. */
        std::chrono::seconds uptime;
    };

    // Convenience function to check current flight mode.
    bool
    Is_flight_mode(ugcs::vsm::proto::Flight_mode);

    bool
    Is_control_mode(ugcs::vsm::proto::Control_mode m);

    // Mission command mapping interface. Used to support current command reporting
    // During mission flight.
    //
    // Here is how it works:
    // 1. mission_upload command contains a list of sub_commands. VSM enumerates it as zero-based.
    //    This becomes the mission_command_id
    // 2. Each mission command can produce any number of vehicle specific commands.
    // 3. The mapping is sent back to server as a response to mission_upload command together with generated mission_id.
    // 4. VSM reports current_vehicle_command to the server during mission flight.
    // 5. Server uses this mapping maps vehicle_specific commands back to mission_commands.
    // 6. mission_id is used to synchronize the command mapping with the server when
    //    the mapping is unknown to VSM (eg. after restart).
    // 7. Server uses the command map only if the map exists and the reported mission_id in telemetry is equal to the
    //    mission_id received together with the map.
    // 8. If server receives different mission_id it drops the current mapping.
    class Command_map
    {
    public:
        // Clear the command mapping and reset mission_id.
        void
        Reset();

        // Set the current_mission_command id as received from ucs.
        // All vehicle specific commands produced by this mission command
        // will map to this ID.
        void
        Set_current_command(int mission_command_id);

        // Map current_mission_command to this vehicle_command_id
        // VSM must call this on each mission item it uploads to the vehicle.
        // Or more precisely: on each command the vehicle is going to report as current command.
        void
        Add_command_mapping(int vehicle_specific_id);

        // Use this function to build mission_id.
        // Used in two scenarios:
        // 1) To report newly uploaded mission_id to server.
        // 2) To calculate mission_id from downloaded mission from the vehicle.
        //    And report as telemetry when VSM has not seen the mission_upload.
        // CRC32 algorithm is used to create a 32bit hash.
        // IMPORTANT:
        // Mission ID is calculated as hash from uploaded vehicle commands
        // in such way that it can be recreated if vehicle supports mission
        // download.
        void
        Accumulate_route_id(uint32_t hash);

        // Get the generated mission_id.
        uint32_t
        Get_route_id();

        // Fill in the mission_upload response payload with accumulated map and mission_id.
        void
        Fill_command_mapping_response(Proto_msg_ptr response);

    private:
        // Mission mapper state.
        int current_mission_command = -1;
        // Current native command mapping to mission subcommands (zero based).
        std::unordered_map<int, int> mission_command_map;
        ugcs::vsm::Crc32 mission_id;
    };

    /** Set system status of this vehicle. Should be called when system
     * status changes, but at least with reasonable granularity to update
     * system uptime.
     */
    void
    Set_system_status(const Sys_status& sys_status);

    /** Get current system status. */
    Sys_status
    Get_system_status() const;

    /** Get vehicle capabilities. */
    Capabilities
    Get_capabilities() const;

    /** Set vehicle capabilities. */
    void
    Set_capabilities(const Capabilities& capabilities);

    /** Get vehicle capability states. */
    Capability_states
    Get_capability_states() const;

    /** Set vehicle capability states. */
    void
    Set_capability_states(const Capability_states& capability_states);

    unsigned int
    Add_subdevice(Subdevice_type);

    Property::Ptr
    Add_telemetry(
        unsigned int subdevice,
        const std::string& name,
        ugcs::vsm::proto::Field_semantic semantic = ugcs::vsm::proto::FIELD_SEMANTIC_DEFAULT,
        uint32_t timeout = 0);

    Property::Ptr
    Add_telemetry(
        unsigned int subdevice,
        const std::string& name,
        ugcs::vsm::Property::Value_type type,
        uint32_t timeout = 0);

    // Remove telemetry field from default set created in Vehicle ctor.
    // Used for vehicles which do not support default fields.
    void
    Remove_telemetry(Property::Ptr&);

    Vsm_command::Ptr
    Add_command(
        unsigned int subdevice,
        const std::string& name,
        bool as_command,
        bool in_mission);

    /* End of user callable methods. */

private:

    /** Calculate system id for this Vehicle. */
    void
    Calculate_system_id();

    Property::Ptr
    Add_telemetry(
        unsigned int subdevice,
        Property::Ptr t);

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

    bool
    Is_model_name_hardcoded();
    /** Type of the autopilot. */
    const mavlink::MAV_AUTOPILOT autopilot;

    /** System state of the vehicle. */
    mavlink::MAV_STATE system_state = mavlink::MAV_STATE::MAV_STATE_UNINIT;

    /** System status of the vehicle. */
    Sys_status sys_status = Sys_status(
            false, false, Sys_status::Control_mode::UNKNOWN,
            Sys_status::State::UNKNOWN, std::chrono::seconds(0));

    /** Current capabilities. */
    Capabilities capabilities;

    /** Current capability states. */
    Capability_states capability_states;

    /** System id of this vehicle as seen by UCS server. */
    mavlink::Mavlink_kind_ugcs::System_id system_id;

    bool model_name_is_hardcoded;

    /** Vehicle state access mutex. */
    static std::mutex state_mutex;

    /** If vehicle is able to know it own altitude origin.*/
    Optional<float> current_altitude_origin;

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

/** Convenience vehicle logging macro. Vehicle should be given by value (no
 * copies will be made). */
#define VEHICLE_LOG(level_, vehicle_, fmt_, ...) \
    _LOG_WRITE_MSG(level_, "[%s:%s] " fmt_, \
            (vehicle_).Get_model_name().c_str(), \
            (vehicle_).Get_serial_number().c_str(), ## __VA_ARGS__)

/** Different level convenience vehicle logging macros. Vehicle should be given
 * by value (no copies will be made). */
// @{
#define VEHICLE_LOG_DBG(vehicle_, fmt_, ...) \
    VEHICLE_LOG(::ugcs::vsm::Log::Level::DEBUGGING, vehicle_, fmt_, ## __VA_ARGS__)

#define VEHICLE_LOG_INF(vehicle_, fmt_, ...) \
    VEHICLE_LOG(::ugcs::vsm::Log::Level::INFO, vehicle_, fmt_, ## __VA_ARGS__)

#define VEHICLE_LOG_WRN(vehicle_, fmt_, ...) \
    VEHICLE_LOG(::ugcs::vsm::Log::Level::WARNING, vehicle_, fmt_, ## __VA_ARGS__)

#define VEHICLE_LOG_ERR(vehicle_, fmt_, ...) \
    VEHICLE_LOG(::ugcs::vsm::Log::Level::ERROR, vehicle_, fmt_, ## __VA_ARGS__)
// @}

} /* namespace vsm */
} /* namespace ugcs */

#endif /* VEHICLE_H_ */
