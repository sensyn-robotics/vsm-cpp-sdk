// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/ucs_vehicle_ctx.h>
#include <vsm/ucs_mission_clear_all_transaction.h>
#include <vsm/ucs_task_upload_transaction.h>
#include <vsm/ucs_vehicle_command_transaction.h>

using namespace vsm;

constexpr std::chrono::seconds Ucs_vehicle_ctx::WRITE_TIMEOUT;

void
Ucs_vehicle_ctx::Enable()
{

}

void
Ucs_vehicle_ctx::Disable()
{
    if (active_transaction) {
        active_transaction->Disable();
        active_transaction = nullptr;
    }
    vehicle.reset();
    completion_context = nullptr;
    request_processor = nullptr;
}

Vehicle::Ptr
Ucs_vehicle_ctx::Get_vehicle()
{
    return vehicle;
}

void
Ucs_vehicle_ctx::Stream_disconnected(const Mavlink_stream::Ptr& mav_stream)
{
    if (active_transaction &&
        active_transaction->Get_mavlink_stream() == mav_stream) {
        /* Abort the transaction, because the result of it is not anymore
         * needed.
         */
        active_transaction->Abort();
    }
}

void
Ucs_vehicle_ctx::Process(
        mavlink::Message<mavlink::MESSAGE_ID::MISSION_CLEAR_ALL>::Ptr message,
        Mavlink_stream::Ptr mav_stream)
{
    Create_or_forward_transaction<Ucs_mission_clear_all_transaction>(message, mav_stream);
}

void
Ucs_vehicle_ctx::Process(
        mavlink::Message<mavlink::MESSAGE_ID::MISSION_COUNT>::Ptr message,
        Mavlink_stream::Ptr mav_stream)
{
    Create_or_forward_transaction<Ucs_task_upload_transaction>(message, mav_stream);
}

void
Ucs_vehicle_ctx::Process(
        mavlink::Message<mavlink::MESSAGE_ID::COMMAND_LONG>::Ptr message,
        Mavlink_stream::Ptr mav_stream)
{
    Create_or_forward_transaction<Ucs_vehicle_command_transaction>(message, mav_stream);
}

void
Ucs_vehicle_ctx::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
                         mavlink::ugcs::Extension>::Ptr message,
        Mavlink_stream::Ptr mav_stream)
{
    Forward_transaction(message, mav_stream);
}

void
Ucs_vehicle_ctx::On_transaction_done()
{
    if (active_transaction) {
        active_transaction->Disable();
        active_transaction = nullptr;
    }
}

void
Ucs_vehicle_ctx::Write_to_ucs_timed_out(
        const Operation_waiter::Ptr& waiter,
        Mavlink_stream::Weak_ptr mav_stream)
{
    auto locked = mav_stream.lock();
    Io_stream::Ref stream = locked ? locked->Get_stream() : nullptr;
    std::string server_info =
            stream ? stream->Get_name() : "already disconnected";
    LOG_DEBUG("Write timeout towards UCS server at [%s] detected from vehicle context.",
            server_info.c_str());
    waiter->Abort();
    if (stream) {
        stream->Close();
    }
}

