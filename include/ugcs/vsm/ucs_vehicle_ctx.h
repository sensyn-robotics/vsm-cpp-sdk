// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_vehicle_ctx.h
 */
#ifndef _UCS_VEHICLE_CTX_H_
#define _UCS_VEHICLE_CTX_H_

#include <ugcs/vsm/vehicle.h>
#include <ugcs/vsm/ucs_transaction.h>

namespace ugcs {
namespace vsm {

/** Context of a vehicle registered to UCS server processor. */
class Ucs_vehicle_ctx: public std::enable_shared_from_this<Ucs_vehicle_ctx> {
    DEFINE_COMMON_CLASS(Ucs_vehicle_ctx, Ucs_vehicle_ctx);

public:

    /** UGCS Mavlink stream type. */
    typedef Mavlink_stream<mavlink::Mavlink_kind_ugcs> Ugcs_mavlink_stream;

    /** Constructor. */
    Ucs_vehicle_ctx(
            Vehicle::Ptr vehicle,
            Request_processor::Ptr request_processor,
            Request_completion_context::Ptr completion_context) :
        vehicle(vehicle),
        request_processor(request_processor),
        completion_context(completion_context)
    {}

    /** Enable the instance. */
    void
    Enable();

    /** Disable the instance. */
    void
    Disable();

    /** Get associated vehicle. */
    Vehicle::Ptr
    Get_vehicle();

    /** Notify the context that particular stream has been disconnected. It
     * does not need to be the stream owned by this context.
     */
    void
    Stream_disconnected(const Ugcs_mavlink_stream::Ptr&);

    // @{
    /** Mavlink message handlers on a vehicle context level. Main purpose is to
     * initiate new transactions, forward messages to existing transaction or
     * reject messages based on simple preliminary checks.
     */
    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_CLEAR_ALL_EX,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_COUNT_EX,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::COMMAND_LONG_EX,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::TAIL_NUMBER_REQUEST,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_INSTALL,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);

    void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_PREFLIGHT,
            mavlink::ugcs::Extension>::Ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream);
    // @}

    void
    Set_current_request_id(uint32_t id)
    {
        current_request_id = id;
    }

private:

    /** Write operations timeout. */
    constexpr static std::chrono::seconds WRITE_TIMEOUT =
            std::chrono::seconds(60);

    /** Template method which makes preliminary checks for a message and
     * forwards it to active transaction on success or responds with
     * negative Mavlink mission ack to server.
     */
    template<class Transaction_type, class Mavlink_message_ptr>
    void
    Create_or_forward_transaction(Mavlink_message_ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream)
    {
        auto ucs_system_id = message->Get_sender_system_id();
        auto ucs_component_id = message->Get_sender_component_id();

        if (active_transaction) {
            if (Is_foreign_message(mav_stream, message)) {
                LOG_WARN("Foreign message %d dropped for active transaction [%s]",
                        message->payload.Get_id(),
                        active_transaction->Get_name().c_str());
                Send_mission_ack(mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID_SEQUENCE,
                        mav_stream, message);
                return;
            }
        } else {
            active_transaction = Transaction_type::Create(
                    mav_stream,
                    ucs_system_id,
                    ucs_component_id,
                    vehicle,
                    completion_context,
                    Make_callback(
                            &Ucs_vehicle_ctx::On_transaction_done,
                            Shared_from_this()));
            active_transaction->Enable();
        }
        active_transaction->Set_current_request_id(current_request_id);
        active_transaction->Process(message);
    }

    /** Template method which makes preliminary checks and forwards message
     * to active transaction on success or responds with negative Mavlink
     * mission ack to server. New transaction is never created.
     */
    template<class Mavlnk_message_ptr>
    void
    Forward_transaction(Mavlnk_message_ptr message,
            Ugcs_mavlink_stream::Ptr mav_stream)
    {
        if (active_transaction && !Is_foreign_message(mav_stream, message)) {
            active_transaction->Set_current_request_id(current_request_id);
            active_transaction->Process(message);
        } else {
            if (active_transaction) {
                LOG_WARN("Foreign message %d dropped for transaction [%s].",
                        message->payload.Get_id(),
                        active_transaction->Get_name().c_str());
            } else {
                LOG_WARN("Foreign message %d dropped, no active transaction.",
                        message->payload.Get_id());
            }
            Send_mission_ack(mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID_SEQUENCE,
                    mav_stream, message);
        }
    }

    /** Done handler of the active transaction. */
    void
    On_transaction_done();

    /**
     * Send Mavlink mission ack message as a response to source message. Supposed
     * to be used to send negative acks for protocol error situations.
     * @param result Result of the ack.
     * @param mav_stream Mavlink stream.
     * @param source_message Mission ack is send as a response to this source
     * message (system and component ids swapped).
     */
    template<class Mavlink_message_ptr>
    void
    Send_mission_ack(mavlink::MAV_MISSION_RESULT result,
            Ugcs_mavlink_stream::Ptr mav_stream,
            Mavlink_message_ptr source_message)
    {
        mavlink::ugcs::Pld_mission_ack_ex ack;

        ack->type = result;
        ack->target_system = source_message->Get_sender_system_id();
        ack->target_component = source_message->Get_sender_component_id();

        Send_response_message(mav_stream, ack, source_message->payload->target_component);
    }

    /** Send Mavlink message from the vehicle.
     * @param mav_stream Stream to use.
     * @param msg Message to send.
     * @param component_id Source component id.
     */
    template<class Mavlink_message>
    void
    Send_message(
            Ugcs_mavlink_stream::Ptr mav_stream,
            const Mavlink_message& msg,
            uint8_t component_id)
    {
        mav_stream->Send_message(
                msg, vehicle->system_id,
                component_id,
                WRITE_TIMEOUT,
                Make_timeout_callback(
                        &Ucs_vehicle_ctx::Write_to_ucs_timed_out,
                        Shared_from_this(),
                        mav_stream),
                        completion_context);
    }

    /** Send Mavlink message from the vehicle.
     * @param mav_stream Stream to use.
     * @param msg Message to send.
     * @param component_id Source component id.
     */
    template<class Mavlink_message>
    void
    Send_response_message(
            Ugcs_mavlink_stream::Ptr mav_stream,
            const Mavlink_message& msg,
            uint8_t component_id)
    {
        mav_stream->Send_response_message(
                msg, vehicle->system_id,
                component_id,
                current_request_id,
                WRITE_TIMEOUT,
                Make_timeout_callback(
                        &Ucs_vehicle_ctx::Write_to_ucs_timed_out,
                        Shared_from_this(),
                        mav_stream),
                        completion_context);
    }

    /**
     * Checks if the message is foreign to currently active transaction.
     * If it is, then its usually a protocol error situation between
     * VSM and UCS.
     * @param message Mavlink message.
     * @return @a true if foreign, @a false otherwise.
     */
    template<class Mavlink_message_ptr>
    bool Is_foreign_message(Ugcs_mavlink_stream::Ptr mav_stream,
            Mavlink_message_ptr message)
    {
        return
            active_transaction->Get_mavlink_stream() != mav_stream ||
            active_transaction->Get_ucs_system_id() != message->Get_sender_system_id() ||
            active_transaction->Get_ucs_component_id() != message->Get_sender_component_id();
    }

    /** Invoked when write operation to the UCS server has timed out. This
     * is quite bad, so close the connection completely.
     */
    void
    Write_to_ucs_timed_out(
            const Operation_waiter::Ptr&,
            Ugcs_mavlink_stream::Weak_ptr);

    Vehicle::Ptr vehicle;

    Request_processor::Ptr request_processor;

    Request_completion_context::Ptr completion_context;

    Ucs_transaction::Ptr active_transaction;

    uint32_t current_request_id;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UCS_VEHICLE_CTX_H_ */
