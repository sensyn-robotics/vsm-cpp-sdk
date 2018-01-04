// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <ugcs/vsm/property.h>
#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/optional.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ugcs {
namespace vsm {

typedef std::shared_ptr<ugcs::vsm::proto::Vsm_message> Proto_msg_ptr;

class Vsm_command: public std::enable_shared_from_this<Vsm_command>
{
    DEFINE_COMMON_CLASS(Vsm_command, Vsm_command)

public:
    Vsm_command(std::string name, bool as_command, bool in_mission);

    Property::Ptr
    Add_parameter(
        std::string,
        ugcs::vsm::proto::Field_semantic semantic = ugcs::vsm::proto::FIELD_SEMANTIC_DEFAULT);

    Property::Ptr
    Add_parameter(std::string name, Property::Value_type type);

    // Fill registration message
    void
    Register(ugcs::vsm::proto::Register_command* command);

    // Fill availability message
    void
    Set_capabilities(ugcs::vsm::proto::Command_availability* msg);

    void
    Set_enabled(bool is_enabled = true);

    void
    Set_available(bool is_available = true);

    int
    Get_id() {return command_id;}

    bool
    Is_capability_state_dirty() {return capability_state_dirty;}

    // create a list of command param values from proto message
    Property_list
    Build_parameter_list(const ugcs::vsm::proto::Device_command &cmd);

    std::string
    Get_name()
    {return name;}

    bool
    Is_mission_item()
        {return in_mission;}

private:
    uint32_t command_id = 0;
    std::unordered_map<int, Property::Ptr> parameters;
    std::string name;

    bool as_command = false;
    bool in_mission = false;

    bool is_available = false;
    bool is_enabled = false;
    bool capability_state_dirty = true;
    friend class Device;
};

class Ucs_request :public Request
{
    DEFINE_COMMON_CLASS(Ucs_request, Request)

public:
    Ucs_request(ugcs::vsm::proto::Vsm_message);

    void
    Complete(
        ugcs::vsm::proto::Status_code = ugcs::vsm::proto::STATUS_OK,
        const std::string& description = std::string());

    Proto_msg_ptr response;
    ugcs::vsm::proto::Vsm_message request;
};

class Device: public std::enable_shared_from_this<Device>
{
    DEFINE_COMMON_CLASS(Device, Device)

public:
    Device(bool create_thread = true);

    typedef Callback_proxy<
            void,
            std::vector<Property::Ptr>>
            Command_handler;

    virtual
    ~Device();

    /** Enable the instance. Should be called right after vehicle instance
     * creation.
     * @throw Invalid_param_exception If vehicle with the same model and and
     * serial number is already registered.
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
    Is_enabled() { return is_enabled;}

    /** Disable copying. */
    Device(const Device &) = delete;

    /** Process requests assigned to vehicle in user thread, i.e. the thread
     * which calls this method. Supposed to be called only when vehicle is
     * created without its own thread.
     *
     */
    void
    Process_requests();

    /** Completion handler type of the request. */
    typedef Callback_proxy<void, uint32_t, Proto_msg_ptr> Response_sender;

    /**
     * Command has arrived from UCS and should be executed by the vehicle.
     */
    void
    On_ucs_message(
        ugcs::vsm::proto::Vsm_message message,
        Response_sender completion_handler = Response_sender(),
        ugcs::vsm::Request_completion_context::Ptr completion_ctx = nullptr);

    // Used by Cucs_processor only.
    // Derived class must override.
    virtual void
    Fill_register_msg(ugcs::vsm::proto::Vsm_message&) = 0;

    template<typename Type>
    void
    Add_property(
        const std::string& name,
        Type value,
        Property::Value_type value_type)
    {
        if (properties.find(name) != properties.end()) {
            VSM_EXCEPTION(Exception, "Property %s already added", name.c_str());
        }
        auto f = Property::Create(name, value, value_type);
        properties.emplace(name, f);
    }

    template<typename Type>
    void
    Add_property(
        const std::string& name,
        ugcs::vsm::proto::Field_semantic semantic,
        Type value)
    {
        if (properties.find(name) != properties.end()) {
            VSM_EXCEPTION(Exception, "Property %s already added", name.c_str());
        }
        auto f = Property::Create(name, semantic, value);
        properties.emplace(name, f);
    }

    uint32_t
    Get_session_id();

    /** Get default completion context of the device. */
    Request_completion_context::Ptr
    Get_completion_ctx();

    static void
    Set_failsafe_actions(Property::Ptr p, std::initializer_list<proto::Failsafe_action> actions);

protected:
    /** Register device instance to UCS processor. After registration is done,
     * UCS servers sees that new vehicle is available.
     */
    void
    Register();

    /** Unregister device instance from UCS processor. */
    void
    Unregister();

    /** Get default processing context of the vehicle. */
    Request_processor::Ptr
    Get_processing_ctx();

    /** Device enable event handler. Can be overridden by derived class,
     * if necessary. Always called in Device context.
     */
    virtual void
    On_enable()
    {};

    /** Device disable event handler. Can be overridden by derived class,
     * if necessary. Always called in Device context.
     */
    virtual void
    On_disable()
    {};

    /**
     * Message from ucs arrived
     * called by VSM SDK in vehicle context and
     * should be overridden by user code.
     */
    virtual void
    Handle_ucs_command(
        Ucs_request::Ptr request);

    void
    Send_ucs_message(Proto_msg_ptr msg);

    Vsm_command::Ptr
    Get_command(int id);

    Property::Ptr
    Add_telemetry(
        const std::string& name,
        ugcs::vsm::proto::Field_semantic sem = ugcs::vsm::proto::FIELD_SEMANTIC_DEFAULT,
        uint32_t timeout = 0);   // timeout in seconds. default: do not specify timeout

    Property::Ptr
    Add_telemetry(
        const std::string& name,
        Property::Value_type type,
        uint32_t timeout = 0);   // timeout in seconds. default: do not specify timeout

    void

    // Remove previously added telemetry field.
    // Invalidates the t_field.
    Remove_telemetry(Property::Ptr& t_field);

    Vsm_command::Ptr
    Add_command(
        const std::string& name,
        bool as_command,
        bool in_mission);

    Request_completion_context::Ptr completion_ctx;
    Request_processor::Ptr processor;
    std::chrono::time_point<std::chrono::system_clock> begin_of_epoch;
    // All properties.
    std::unordered_map<std::string, Property::Ptr> properties;

    void
    Add_status_message(const std::string& m);

    class Commit_scope {
    public:
        Commit_scope(Device& d):d(d) {}
        ~Commit_scope() {d.Commit_to_ucs();}
    private:
        Device& d;
    };

    // Add this in each scope which requires a call to Commit_to_ucs().
    // This guarantees the commit on scope leave and you do not need to
    // add Commit_to_ucs() before each return.
    #define CREATE_COMMIT_SCOPE auto auto_device_commit_scope = Commit_scope(*this)

    void
    Commit_to_ucs();

private:
    Request_worker::Ptr worker;

    // All registered telemetry fields.
    std::vector<Property::Ptr> telemetry_fields;
    // All commands supported by device
    std::unordered_map<int, Vsm_command::Ptr> commands;
    // Status messages to be sent to ucs.
    std::list<std::string> device_status_messages;

    uint32_t my_handle = 0;

    /** Is vehicle enabled. */
    bool is_enabled = false;
};

/** Convenience vehicle logging macro. Vehicle should be given by value (no
 * copies will be made). */
#define DEVICE_LOG(level_, vehicle_, fmt_, ...) \
    _LOG_WRITE_MSG(level_, "[%d] " fmt_, \
            (vehicle_).Get_session_id(), ## __VA_ARGS__)

/** Different level convenience vehicle logging macros. Vehicle should be given
 * by value (no copies will be made). */
// @{
#define DEVICE_LOG_DBG(vehicle_, fmt_, ...) \
    DEVICE_LOG(::ugcs::vsm::Log::Level::DEBUGGING, vehicle_, fmt_, ## __VA_ARGS__)

#define DEVICE_LOG_INF(vehicle_, fmt_, ...) \
    DEVICE_LOG(::ugcs::vsm::Log::Level::INFO, vehicle_, fmt_, ## __VA_ARGS__)

#define DEVICE_LOG_WRN(vehicle_, fmt_, ...) \
    DEVICE_LOG(::ugcs::vsm::Log::Level::WARNING, vehicle_, fmt_, ## __VA_ARGS__)

#define DEVICE_LOG_ERR(vehicle_, fmt_, ...) \
    DEVICE_LOG(::ugcs::vsm::Log::Level::ERROR, vehicle_, fmt_, ## __VA_ARGS__)
// @}

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _DEVICE_H_ */
