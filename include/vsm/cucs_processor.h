// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file cucs_processor.h
 *
 * UCS processor manages communications with UCS server and registered
 * vehicles.
 */

#ifndef CUCS_PROCESSOR_H_
#define CUCS_PROCESSOR_H_

#include <vsm/request_worker.h>
#include <vsm/vehicle.h>
#include <vsm/socket_processor.h>
#include <vsm/ucs_vehicle_ctx.h>
#include <vsm/mavlink_stream.h>
#include <vsm/adsb_report.h>
#include <unordered_set>

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
    Register_vehicle(Vehicle::Ptr);

    /** Unregistration of a vehicle instance in the processor. */
    void
    Unregister_vehicle(Vehicle::Ptr);

    /** Send ADS-B report to all currently connected UCS servers. */
    void
    Send_adsb_report(const Adsb_report&);

private:

    enum {
        /** Used when system id from vehicle side is not known. */
        DEFAULT_SYSTEM_ID = 1,

        /** Used when component id from vehicle side is not known. */
        DEFAULT_COMPONENT_ID = 1,

        /** Maximum number of pending ADS-B reports allowed. */
        MAX_ADSB_REPORTS_PENDING = 16,
    };

    /** Write operations timeout. */
    constexpr static std::chrono::seconds WRITE_TIMEOUT =
            std::chrono::seconds(60);

    /** Standard worker is enough, because there are no custom threads
     * in Cucs processor.
     */
    Request_worker::Ptr worker;

    /** Cucs processor completion context. */
    Request_completion_context::Ptr completion_ctx;

    /** Currently established UCS server connections and their read operation. */
    std::unordered_map<Mavlink_stream::Ptr, Operation_waiter> ucs_connections;

    /** System ids of registered vehicles and their contexts. */
    std::unordered_map<mavlink::System_id, Ucs_vehicle_ctx::Ptr> vehicles;

    /** Stream which listens for incoming CUCS connections. */
    Socket_processor::Socket_listener::Ref cucs_listener;

    /** Cucs listener opening operation. */
    Operation_waiter cucs_listener_op;

    /** Current accept operation, if any. */
    Operation_waiter accept_op;

    /** Number of pending ADS-B reports. When this value reaches the
     * MAX_ADSB_REPORTS_PENDING limit, new reports are sent synchronously to
     * avoid CUCS processor overload.
     */
    std::atomic_size_t pending_adsb_reports;

    virtual void
    On_enable() override;

    virtual void
    On_disable() override;

    void
    Process_on_disable(Request::Ptr);

    /** Listening to UCS server started. */
    void
    On_listening_started(Socket_processor::Socket_listener::Ref listener,
            Io_result result);

    /** Incoming connection from UCS arrived. */
    void
    On_incoming_connection(Socket_processor::Stream::Ref stream, Io_result result);

    /** Register handlers for all supported Mavlink packets sent from UCS server. */
    void
    Register_mavlink_handlers(Mavlink_stream::Ptr);

    /** Schedule next read operation for a stream. */
    void
    Schedule_next_read(Mavlink_stream::Ptr&, Operation_waiter&);

    /** Read operation for a given UCS connection stream completed. */
    void
    Read_completed(Io_buffer::Ptr, Io_result, Mavlink_stream::Ptr);


    /**
     * Register handler for specific Mavlink message id.
     * @param mav_stream Stream for handler registration.
     */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extension_type = mavlink::Extension>
    void
    Register_mavlink_message(Mavlink_stream::Ptr mav_stream)
    {
        mav_stream->Get_demuxer().Register_handler<message_id, Extension_type>(
                Make_mavlink_demuxer_handler<message_id, Extension_type>(
                        &Cucs_processor::On_mavlink_message<message_id, Extension_type>,
                        Shared_from_this(),
                        mav_stream));
    }

    void
    Start_listening();

    void
    Accept_next_connection();

    void
    On_register_vehicle(Request::Ptr, Vehicle::Ptr, bool* success);

    void
    On_unregister_vehicle(Request::Ptr, Vehicle::Ptr, bool* success);

    void
    On_send_adsb_report(Adsb_report, Request::Ptr);

    bool
    On_default_mavlink_message_handler(mavlink::MESSAGE_ID_TYPE, mavlink::System_id,
            uint8_t, Mavlink_stream::Ptr);

    bool
    On_heartbeat_timer();

    /** Default handler for Mavlink messages which have target_system field
     * which is mapped to vehicle system id.
     * Custom behavior is achieved by providing template specializations, for
     * example, messages which do not have target_system field at all.
     * @param message Mavlink message to process.
     * @param mav_stream Stream through which the message was received.
     */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extention_type>
    void
    On_mavlink_message(
            typename mavlink::Message<message_id, Extention_type>::Ptr message,
            Mavlink_stream::Ptr mav_stream)
    {
        uint8_t vehicle_system_id = message->payload->target_system;
        auto iter = vehicles.find(vehicle_system_id);
        if (iter == vehicles.end()) {
            LOG_DEBUG("Vehicle with system id %d not registered (message %d)",
                    vehicle_system_id, message->payload.Get_id());
            /* Respond nack to server. */
            mavlink::Pld_mission_ack ack;
            ack->target_system = message->Get_sender_system_id();
            ack->target_component = message->Get_sender_component_id();
            ack->type = mavlink::MAV_MISSION_RESULT::MAV_MISSION_ERROR;
            Send_message(mav_stream, ack, vehicle_system_id,
                    message->payload->target_component);
        } else {
            /* Pass the message to the vehicle context for further processing. */
            iter->second->Process(message, mav_stream);
        }
    }

    void
    Register_vehicle_telemetry(Ucs_vehicle_ctx::Ptr);

    void
    Unregister_vehicle_telemetry(Ucs_vehicle_ctx::Ptr);

    /** Telemetry handler called from vehicle context. */
    template<class Mavlink_payload>
    void
    On_telemetry(const Mavlink_payload* message, Ucs_vehicle_ctx::Weak_ptr ctx)
    {
        auto request = Request::Create();
        auto proc_handler = Make_callback(
                &Cucs_processor::On_telemetry_handle<Mavlink_payload>,
                Shared_from_this(),
                *message,
                ctx,
                request);
        request->Set_processing_handler(proc_handler);
        Submit_request(request);
    }

    /** Telemetry handlers called from UCS processor context. */
    template<class Mavlink_payload>
    void
    On_telemetry_handle(Mavlink_payload message, Ucs_vehicle_ctx::Weak_ptr ctx,
            Request::Ptr request)
    {
        Ucs_vehicle_ctx::Ptr strong_ctx = ctx.lock();
        if (strong_ctx) {
            ASSERT(strong_ctx->Get_vehicle());
            Broadcast_mavlink_message(message, strong_ctx->Get_vehicle()->system_id,
                    DEFAULT_COMPONENT_ID);
        }
        request->Complete();
    }

    /** Send Mavlink message. */
    void
    Send_message(
            const Mavlink_stream::Ptr mav_stream,
            const mavlink::Payload_base& payload,
            mavlink::System_id system_id,
            uint8_t component_id);

    /** Invoked when write operation to the UCS server has timed out. This
     * is quite bad, so close the connection completely.
     */
    void
    Write_to_ucs_timed_out(
            const Operation_waiter::Ptr&,
            Mavlink_stream::Weak_ptr);

    /** Broadcast a message to all currently connected UCS
     * servers. */
    void
    Broadcast_mavlink_message(const mavlink::Payload_base&,
            Mavlink_demuxer::System_id, Mavlink_demuxer::Component_id);

    /** Cucs processor singleton instance. */
    static Singleton<Cucs_processor> singleton;

    /** Heartbeat timer. Used to send heartbeat messages from all registered
     * vehicles to all connected Mavlink streams (UCS servers).
     */
    Timer_processor::Timer::Ptr heartbeat_timer;
};

} /* namespace vsm */
#endif /* CUCS_PROCESSOR_H_ */
