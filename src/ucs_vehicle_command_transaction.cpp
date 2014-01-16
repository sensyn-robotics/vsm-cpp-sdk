// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/ucs_vehicle_command_transaction.h>

using namespace vsm;

std::string
Ucs_vehicle_command_transaction::Get_name() const
{
    return "VEHICLE_COMMAND";
}

void
Ucs_vehicle_command_transaction::On_disable()
{
    if (vehicle_command) {
        vehicle_command->Abort();
        vehicle_command = nullptr;
    }
}

void
Ucs_vehicle_command_transaction::On_abort()
{
    if (!vehicle_command) {
        Done();
    }
}

void
Ucs_vehicle_command_transaction::Process(
        mavlink::Message<mavlink::MESSAGE_ID::COMMAND_LONG>::Ptr message)
{
    if (vehicle_command) {
        /* Already in progress. */
        Send_command_ack(
                mavlink::MAV_RESULT::MAV_RESULT_TEMPORARILY_REJECTED,
                message->payload);
        return;
    }

    Vehicle_command::Type type;

    bool unsupported = false;

    switch (message->payload->command) {
    case mavlink::MAV_CMD::MAV_CMD_OVERRIDE_GOTO:
        switch (static_cast<int>(message->payload->param1)) {
        case mavlink::MAV_GOTO::MAV_GOTO_DO_HOLD:
            type = Vehicle_command::Type::HOLD;
            break;
        case mavlink::MAV_GOTO::MAV_GOTO_DO_CONTINUE:
            type = Vehicle_command::Type::GO;
            break;
        default:
            unsupported = true;
        }
        break;
    default:
        unsupported = true;
    }

    if (unsupported) {
        Send_command_ack(
                mavlink::MAV_RESULT::MAV_RESULT_UNSUPPORTED,
                message->payload);
        Done();
        return;
    }

    /* Start new transaction. */
    command = message->payload;
    auto completion_handler =
            Make_callback(&Ucs_vehicle_command_transaction::On_vehicle_command_completed,
                    Shared_from_this(), Vehicle_request::Result::NOK);
    vehicle_command = Vehicle_command_request::Create(completion_handler,
            completion_context, type);
    vehicle->Submit_vehicle_request(vehicle_command);
}

void
Ucs_vehicle_command_transaction::On_vehicle_command_completed(Vehicle_request::Result result)
{
    LOG_DEBUG("Command completed for vehicle %d, result %d",
            vehicle->system_id,
            static_cast<int>(result));

    if (!Is_aborted()) {
        Send_command_ack(result == Vehicle_request::Result::OK ?
                mavlink::MAV_RESULT::MAV_RESULT_ACCEPTED :
                mavlink::MAV_RESULT::MAV_RESULT_FAILED,
                command);
    }
    vehicle_command = nullptr;
    Done();
}
