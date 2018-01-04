// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file cucs_processor.h
 *
 * UCS processor manages communications with UCS server and registered
 * vehicles.
 */

#ifndef _CUCS_PROCESSOR_H_
#define _CUCS_PROCESSOR_H_

#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/device.h>
#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/mavlink_stream.h>
#include <ugcs/vsm/transport_detector.h>
#include <ucs_vsm_proto.h>
#include <unordered_set>
#include <map>

namespace ugcs {
namespace vsm {

/** Handles interactions with CUCS. Intermediate version. */
class Cucs_processor: public Request_processor {
    DEFINE_COMMON_CLASS(Cucs_processor, Request_container);

public:
    /**
     * Default constructor.
     */
    Cucs_processor();

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Registration of a vehicle instance in the processor. */
    void
    Register_device(Device::Ptr);

    /** Unregistration of a vehicle instance in the processor. */
    void
    Unregister_device(uint32_t handle);

    void
    Send_ucs_message(uint32_t handle, Proto_msg_ptr message);

private:
    /** Write operations timeout. */
    constexpr static std::chrono::seconds WRITE_TIMEOUT = std::chrono::seconds(60);

    /** How long to wait for Register_peer from server. */
    constexpr static std::chrono::seconds REGISTER_PEER_TIMEOUT = std::chrono::seconds(10);

    // Protobuf message of size above this is considered an attack.
    constexpr static size_t PROTO_MAX_MESSAGE_LEN = 1000000;

    /** Standard worker is enough, because there are no custom threads
     * in Cucs processor.
     */
    Request_worker::Ptr worker;

    /** Cucs processor completion context. */
    Request_completion_context::Ptr completion_ctx;

    uint32_t ucs_id_counter;

    uint32_t
    Get_next_id() { return ucs_id_counter++; }

    typedef struct {
        size_t stream_id;
        Io_stream::Ref stream;
        Socket_address::Ptr address;
        Optional<uint32_t> ucs_id;
        Operation_waiter read_waiter;

        // wireformat reader FSM
        bool reading_header = true;
        size_t to_read = 1;
        size_t message_size = 0;
        int shift = 0;

        // This is primary connection with this server.
        // VSM will use this as primary connection.
        bool primary = false;

        // List of device_ids which has successfully registered on this connection.
        // Used to avoid sending telemetry to connections which are not ready for it.
        std::unordered_set<uint32_t> registered_devices;

        // Map request_id -> device_id used to keep track of pending registrations.
        std::unordered_map<uint32_t, uint32_t> pending_registrations;
    } Server_context;

    typedef struct {
        Device::Ptr vehicle;
        // field_id -> field value
        std::unordered_map<uint32_t, ugcs::vsm::proto::Telemetry_field> telemetry_cache;
        std::unordered_map<uint32_t, ugcs::vsm::proto::Command_availability> availability_cache;

        // created on vehicle register and not updated any more.
        // I.e. for now vehicle in not allowed to modify its
        // telemetry, commands, or props after registration
        ugcs::vsm::proto::Vsm_message registration_message;
    } Vehicle_context;

    /** Currently established UCS server connections. Indexed by stream_id*/
    std::unordered_map<
        uint32_t,
        Server_context> ucs_connections;

    /** System ids of registered vehicles and their contexts. */
    std::unordered_map<
        uint32_t,
        Vehicle_context> vehicles;

    /** Dedicated detector only for server connections.
     * Because the singleton Transport_detector is used by vehicles and can be disabled. */
    Transport_detector::Ptr ucs_connector;

    /** Leave transport detector on when there are no server connections. */
    bool transport_detector_on_when_diconnected = false;

    virtual void
    On_enable() override;

    virtual void
    On_disable() override;

    void
    Process_on_disable(Request::Ptr);

    /** Incoming connection from UCS arrived. */
    void
    On_incoming_connection(std::string, int, Socket_address::Ptr, Io_stream::Ref);

    /** Schedule next read operation for a stream. */
    void
    Schedule_next_read(Server_context& sc);

    /** Read operation for a given UCS connection stream completed. */
    void
    Read_completed(
            Io_buffer::Ptr,
            Io_result,
            size_t stream_id);

    /** Read operation for a given UCS connection stream completed. */
    void
    Write_completed(
            Io_result,
            size_t stream_id);

    void
    On_register_vehicle(Request::Ptr, Device::Ptr);

    void
    On_unregister_vehicle(Request::Ptr, uint32_t handle);

    void
    On_send_ucs_message(Request::Ptr request, uint32_t handle, Proto_msg_ptr message);

    void
    Send_ucs_message(
        uint32_t stream_id,
        ugcs::vsm::proto::Vsm_message& message);

    void
    Send_ucs_message_ptr(uint32_t stream_id, Proto_msg_ptr message);

    // Send message to all connected ucs.
    // Prefers locally connected.
    void
    Broadcast_message_to_ucs(ugcs::vsm::proto::Vsm_message& message);

    void
    On_ucs_message(
        uint32_t stream_id,
        ugcs::vsm::proto::Vsm_message message);

    void
    On_ucs_message_vehicle(
        uint32_t stream_id,
        ugcs::vsm::proto::Vsm_message message);

    void
    Send_vehicle_registrations(
        Server_context& ctx);

    Device::Ptr
    Get_device(uint32_t device_id);

    void
    Close_ucs_stream(size_t stream_id);

    /** Cucs processor singleton instance. */
    static Singleton<Cucs_processor> singleton;
};

} /* namespace vsm */
} /* namespace ugcs */
#endif /* _CUCS_PROCESSOR_H_ */
