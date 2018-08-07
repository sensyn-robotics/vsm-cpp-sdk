// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _UGCS_VSM_DEVICE_H_
#define _UGCS_VSM_DEVICE_H_

#include <ugcs/vsm/property.h>
#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/optional.h>
#include <ugcs/vsm/subsystem.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ugcs {
namespace vsm {

typedef std::shared_ptr<ugcs::vsm::proto::Vsm_message> Proto_msg_ptr;

class Ucs_request :public Request
{
    DEFINE_COMMON_CLASS(Ucs_request, Request)

public:
    Ucs_request(ugcs::vsm::proto::Vsm_message);

    void
    Complete(
        ugcs::vsm::proto::Status_code = ugcs::vsm::proto::STATUS_OK,
        const std::string& description = std::string());

    // Device can modify device_response of this message
    Proto_msg_ptr response = nullptr;

    // Used to send in-progress status for the request.
    uint32_t stream_id = 0;

    ugcs::vsm::proto::Vsm_message request;
};

class Device: public std::enable_shared_from_this<Device>
{
    DEFINE_COMMON_CLASS(Device, Device)

public:
    Device(proto::Device_type type, bool create_thread = true);

    typedef Callback_proxy<
            void,
            std::vector<Property::Ptr>>
            Command_handler;

    /** Completion handler type of the request. */
    typedef Callback_proxy<void, uint32_t, Proto_msg_ptr> Response_sender;

    virtual
    ~Device();

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
    void
    Register(ugcs::vsm::proto::Vsm_message&);

    // Create/replace property value. By default type is derived from value.
    template<typename Type>
    Property::Ptr
    Set_property(
        const std::string& name,
        Type value,
        proto::Field_semantic semantic = proto::FIELD_SEMANTIC_DEFAULT)
    {
        auto it = properties.find(name);
        if (it == properties.end()) {
            auto f = Property::Create(name, value, semantic);
            properties.emplace(name, f);
            return f;
        } else {
            it->second->Set_value(value);
            return it->second;
        }
    }

    uint32_t
    Get_session_id();

    /** Get default completion context of the device. */
    Request_completion_context::Ptr
    Get_completion_ctx();

    static void
    Set_failsafe_actions(Property::Ptr p, std::initializer_list<proto::Failsafe_action> actions);

    /** Register device instance to UCS processor. After registration is done,
     * UCS servers sees that new vehicle is available.
     */
    void
    Register();

    /** Unregister device instance from UCS processor. */
    void
    Unregister();

    /** Returns true if vehicle is registered with ucs. */
    bool
    Is_registered();

    std::string
    Dump_command(const ugcs::vsm::proto::Device_command &);

    Subsystem::Ptr
    Add_subsystem(proto::Subsystem_type);

protected:
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

    /** Sends Device_response with status code==STATUS_IN_PROGRESS with optional progress and description.
     * Used to display progress-bar and/or some description.
     * @param progress float in range [0..1]. If progress < 0, it is not sent.
     * @param description Description of current progress. Not sent if empty.
     */
    void
    Report_progress(
        Ucs_request::Ptr request,
        float progress = -1.0,
        const std::string& description = std::string());

    void
    Send_ucs_message(Proto_msg_ptr msg);

    Vsm_command::Ptr
    Get_command(int id);

    // Append to the list of status messages. Commit_to_ucs will push all to ucs.
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

    Request_completion_context::Ptr completion_ctx;
    Request_processor::Ptr processor;
    std::chrono::time_point<std::chrono::system_clock> begin_of_epoch;

    std::vector<Subsystem::Ptr> subsystems;

private:
    Request_worker::Ptr worker;

    // Status messages to be sent to ucs.
    std::list<std::string> device_status_messages;

    uint32_t my_handle = 0;

    /** Is vehicle enabled. */
    bool is_enabled = false;

    const proto::Device_type device_type;

    std::unordered_map<std::string, Property::Ptr> properties;
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

#endif /* _UGCS_VSM_DEVICE_H_ */
