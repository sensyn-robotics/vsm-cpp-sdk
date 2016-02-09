// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/ucs_transaction.h>

using namespace ugcs::vsm;

constexpr std::chrono::seconds Ucs_transaction::WRITE_TIMEOUT;

Ucs_transaction::Ucs_transaction(
        Ugcs_mavlink_stream::Ptr mav_stream,
        typename mavlink::Mavlink_kind_ugcs::System_id ucs_system_id,
        uint8_t ucs_component_id,
        Vehicle::Ptr vehicle,
        Request_completion_context::Ptr completion_context,
        Done_handler done_handler) :
            mav_stream(mav_stream),
            ucs_system_id(ucs_system_id),
            ucs_component_id(ucs_component_id),
            vehicle(vehicle),
            completion_context(completion_context),
            done_handler(done_handler) {}

void
Ucs_transaction::Enable()
{
    On_enable();
}

void
Ucs_transaction::Disable()
{
    On_disable();
    mav_stream = nullptr;
    vehicle = nullptr;
    completion_context = nullptr;
    done_handler = Done_handler();
}

void
Ucs_transaction::Abort()
{
    aborted = true;
    On_abort();
}

typename mavlink::Mavlink_kind_ugcs::System_id
Ucs_transaction::Get_ucs_system_id() const
{
    return ucs_system_id;
}

uint8_t
Ucs_transaction::Get_ucs_component_id() const
{
    return ucs_component_id;
}

Ucs_transaction::Ugcs_mavlink_stream::Ptr
Ucs_transaction::Get_mavlink_stream()
{
    return mav_stream;
}

bool
Ucs_transaction::Is_aborted() const
{
    return aborted;
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_CLEAR_ALL_EX,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_COUNT_EX,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::COMMAND_LONG_EX,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_INSTALL,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_PREFLIGHT,
                         mavlink::ugcs::Extension>::Ptr msg)
{
    Default_message_processing(msg);
}

void
Ucs_transaction::Done()
{
    if (done_handler) {
        done_handler();
    }
}

void
Ucs_transaction::Send_mission_ack(
    mavlink::MAV_MISSION_RESULT result,
    uint8_t vehicle_component_id)
{
    mavlink::ugcs::Pld_mission_ack_ex ack;

    ack->type = result;
    ack->target_system = ucs_system_id;
    ack->target_component = ucs_component_id;

    Send_response_message(ack, vehicle->system_id, vehicle_component_id);
}

void
Ucs_transaction::Send_command_ack(
    mavlink::MAV_RESULT result,
    const mavlink::ugcs::Pld_command_long_ex& command)
{
    mavlink::Pld_command_ack ack;

    ack->command = command->command;
    ack->result = result;

    Send_response_message(ack, vehicle->system_id, command->target_component);
}

void
Ucs_transaction::Send_adsb_ack(
    mavlink::MAV_RESULT result)
{
    mavlink::ugcs::Pld_adsb_transponder_response ack;

    ack->result = result;

    Send_response_message(ack, vehicle->system_id, 0);
}

void
Ucs_transaction::Send_response_message(
        const mavlink::Payload_base& payload,
        typename mavlink::Mavlink_kind_ugcs::System_id system_id,
        uint8_t component_id)
{
    mav_stream->Send_response_message(
            payload,
            system_id,
            component_id,
            current_request_id,
            WRITE_TIMEOUT,
            Make_timeout_callback(
                    &Ucs_transaction::Write_to_ucs_timed_out,
                    Shared_from_this(),
                    mav_stream),
            completion_context);
}

void
Ucs_transaction::Send_message(
        const mavlink::Payload_base& payload,
        typename mavlink::Mavlink_kind_ugcs::System_id system_id,
        uint8_t component_id)
{
    mav_stream->Send_message(
            payload,
            system_id,
            component_id,
            WRITE_TIMEOUT,
            Make_timeout_callback(
                    &Ucs_transaction::Write_to_ucs_timed_out,
                    Shared_from_this(),
                    mav_stream),
            completion_context);
}

void
Ucs_transaction::Write_to_ucs_timed_out(
        const Operation_waiter::Ptr& waiter,
        Ugcs_mavlink_stream::Weak_ptr mav_stream)
{
    auto locked = mav_stream.lock();
    Io_stream::Ref stream = locked ? locked->Get_stream() : nullptr;
    std::string server_info =
            stream ? stream->Get_name() : "already disconnected";
    LOG_DEBUG("Write timeout towards UCS server at [%s] detected from UCS transaction.",
            server_info.c_str());
    waiter->Abort();
    if (stream) {
        stream->Close();
    }
}
