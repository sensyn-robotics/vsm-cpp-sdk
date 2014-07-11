// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_transaction.h
 */
#ifndef _UCS_TRANSACTION_H_
#define _UCS_TRANSACTION_H_

#include <ugcs/vsm/mavlink_stream.h>
#include <ugcs/vsm/vehicle.h>

namespace ugcs {
namespace vsm {

/** Transaction which is initiated by UCS towards a vehicle. Only
 * one transaction per vehicle at a time is allowed, others are rejected.
 */
class Ucs_transaction: public std::enable_shared_from_this<Ucs_transaction> {
    DEFINE_COMMON_CLASS(Ucs_transaction, Ucs_transaction);
public:

    /** UCS transaction done handler. */
    typedef Callback_proxy<void> Done_handler;

    /** UGCS Mavlink stream type. */
    typedef Mavlink_stream<mavlink::Mavlink_kind_ugcs> Ugcs_mavlink_stream;

    /** Construct UCS transaction bound to Mavlink stream, UCS system/component id,
     * vehicle and completion context.
     */
    Ucs_transaction(
            Ugcs_mavlink_stream::Ptr mav_stream,
            typename mavlink::Mavlink_kind_ugcs::System_id ucs_system_id,
            uint8_t ucs_component_id,
            Vehicle::Ptr vehicle,
            Request_completion_context::Ptr completion_context,
            Done_handler done_handler = Done_handler());

    /** Enable transaction. */
    void
    Enable();

    /** Disable transaction. */
    void
    Disable();

    /** Abort the transaction. Note that it does not always cause the
     * transaction to be immediately completed (i.e. done), because there could
     * be vehicle request submitted. In this case, transaction waits for vehicle
     * request to be completed and then done handler is invoked.
     */
    void
    Abort();

    /** Get the system id of the UCS server. */
    typename mavlink::Mavlink_kind_ugcs::System_id
    Get_ucs_system_id() const;

    /** Get the component id of the UCS server. */
    uint8_t
    Get_ucs_component_id() const;

    /** Get associated Mavlink stream. */
    Ugcs_mavlink_stream::Ptr
    Get_mavlink_stream();

    /** Get human readable name of the transaction. */
    virtual std::string
    Get_name() const = 0;

    /** Return transaction abort status. */
    bool
    Is_aborted() const;

    // @{
    /** Process methods for all currently supported Mavlink messages. Does
     * nothing by default, should be overridden by derived class if custom
     * behavior is needed. By default - sends mission NACK. */
    virtual void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_CLEAR_ALL_EX,
            mavlink::ugcs::Extension>::Ptr);

    virtual void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_COUNT_EX,
            mavlink::ugcs::Extension>::Ptr);

    virtual void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
            mavlink::ugcs::Extension>::Ptr);

    virtual void
    Process(mavlink::Message<mavlink::ugcs::MESSAGE_ID::COMMAND_LONG_EX,
            mavlink::ugcs::Extension>::Ptr);
    // @}

protected:

    /** Enable event, to be overridden in subclass, if necessary. */
    virtual void
    On_enable() {};

    /** Disable event, to be overridden in subclass, if necessary. */
    virtual void
    On_disable() {};

    /** Abort event, to be overridden in subclass, if necessary. */
    virtual void
    On_abort() {};

    /** Should be always called by derived class when transaction is finished
     * either with or without any errors.
     */
    void
    Done();

    /**
     * Send Mavlink mission ack message toward UCS server associated with
     * this transaction.
     * @param result Result of the ack.
     * @param vehicle_component_id Component id of the vehicle which sends ack.
     */
    void
    Send_mission_ack(mavlink::MAV_MISSION_RESULT result, uint8_t vehicle_component_id);

    /**
     * Send Mavlink mission ack message as a response to source message. Supposed
     * to be used to send negative acks for protocol error situations.
     * @param result Result of the ack.
     * @param source_message Mission ack is send as a response to this source
     * message (system and component ids swapped).
     */
    template<class Mavlink_message_ptr>
    void
    Send_mission_ack(mavlink::MAV_MISSION_RESULT result, Mavlink_message_ptr source_message)
    {
        mavlink::ugcs::Pld_mission_ack_ex ack;

        ack->type = result;
        ack->target_system = source_message->Get_sender_system_id();
        ack->target_component = source_message->Get_sender_component_id();

        Send_message(ack, vehicle->system_id,
                source_message->payload->target_component);
    }

    /**
     * Send Mavlink command acknowledgment.
     * @param result Result of the command execution.
     * @param command Original command.
     */
    void
    Send_command_ack(mavlink::MAV_RESULT result, const mavlink::ugcs::Pld_command_long_ex& command);

    /**
     * Send Mavlink message towads UCS server.
     * @param payload Message to send.
     * @param system_id System id.
     * @param component_id Component id.
     */
    void
    Send_message(
            const mavlink::Payload_base& payload,
            typename mavlink::Mavlink_kind_ugcs::System_id system_id,
            uint8_t component_id);

    /** Mavlink stream with UCS server. */
    Ugcs_mavlink_stream::Ptr mav_stream;

    /** System id of the UCS server which has initiated the transaction. */
    const typename mavlink::Mavlink_kind_ugcs::System_id ucs_system_id;

    /** Component id of the UCS server which has initiated the transaction. */
    const uint8_t ucs_component_id;

    /** Vehicle processed by this transaction. */
    Vehicle::Ptr vehicle;

    /** Completion context. */
    Request_completion_context::Ptr completion_context;

private:

    /** Write operations timeout. */
    constexpr static std::chrono::seconds WRITE_TIMEOUT =
            std::chrono::seconds(60);

    /** Default processing of a Mavlink message if it is not overridden by
     * derived transaction class.
     */
    template<class Message_ptr>
    void
    Default_message_processing(Message_ptr msg)
    {
        LOG_WARN("Unsupported message %d in [%s] transaction for vehicle "
                "[%s:%s]", msg->payload.Get_id(), Get_name().c_str(),
                vehicle->Get_model_name().c_str(),
                vehicle->Get_serial_number().c_str());
        Send_mission_ack(mavlink::MAV_MISSION_UNSUPPORTED, msg);
    }

    /** Invoked when write operation to the UCS server has timed out. This
     * is quite bad, so close the connection completely.
     */
    void
    Write_to_ucs_timed_out(
            const Operation_waiter::Ptr&,
            Ugcs_mavlink_stream::Weak_ptr);

    /** Done handler. */
    Done_handler done_handler;

    /** Indicate whether transaction is aborted. */
    bool aborted = false;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UCS_TRANSACTION_H_ */
