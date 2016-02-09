// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/ucs_vehicle_command_transaction.h>

using namespace ugcs::vsm;

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
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::COMMAND_LONG_EX,
                         mavlink::ugcs::Extension>::Ptr message)
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
        if (message->payload->param1 == mavlink::MAV_GOTO::MAV_GOTO_DO_HOLD) {
            if (message->payload->param2 ==
                    mavlink::MAV_GOTO::MAV_GOTO_HOLD_AT_CURRENT_POSITION) {
                type = Vehicle_command::Type::PAUSE_MISSION;
            } else {
                unsupported = true;
            }
        } else if (message->payload->param1 ==
                mavlink::MAV_GOTO::MAV_GOTO_DO_CONTINUE) {
            type = Vehicle_command::Type::RESUME_MISSION;
        } else {
            unsupported = true;
        }
        break;
    case mavlink::MAV_CMD::MAV_CMD_COMPONENT_ARM_DISARM:
        if (message->payload->param1) {
            type = Vehicle_command::Type::ARM;
        } else {
            type = Vehicle_command::Type::DISARM;
        }
        break;
    case mavlink::MAV_CMD::MAV_CMD_DO_SET_MODE:
        if (message->payload->param1 ==
                mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_AUTO_ENABLED) {
            type = Vehicle_command::Type::AUTO_MODE;
        } else if (message->payload->param1 ==
                mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_MANUAL_INPUT_ENABLED) {
            type = Vehicle_command::Type::MANUAL_MODE;
        } else if (message->payload->param1 ==
                mavlink::MAV_MODE_FLAG::MAV_MODE_FLAG_GUIDED_ENABLED) {
            type = Vehicle_command::Type::GUIDED_MODE;
        } else {
            unsupported = true;
        }
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_RETURN_HOME:
        type = Vehicle_command::Type::RETURN_HOME;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_TAKEOFF_EX:
        type = Vehicle_command::Type::TAKEOFF;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_LAND_EX:
        type = Vehicle_command::Type::LAND;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_EMERGENCY_LAND:
        type = Vehicle_command::Type::EMERGENCY_LAND;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_TRIGGER:
        type = Vehicle_command::Type::CAMERA_TRIGGER;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_WAYPOINT_EX:
        type = Vehicle_command::Type::WAYPOINT;
        break;
    case mavlink::ugcs::MAV_CMD::MAV_CMD_DO_ASDB_OPERATING:
        type = Vehicle_command::Type::ADSB_OPERATING;
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
        completion_context, type, message->payload);
    vehicle->Submit_vehicle_request(vehicle_command);
}

void
Ucs_vehicle_command_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_INSTALL,
                         mavlink::ugcs::Extension>::Ptr message)
{
    if (vehicle_command) {
        /* Already in progress. */
        Send_adsb_ack(mavlink::MAV_RESULT::MAV_RESULT_TEMPORARILY_REJECTED);
        return;
    }

    /* Start new transaction. */
    auto completion_handler =
            Make_callback(&Ucs_vehicle_command_transaction::On_adsb_command_completed,
                    Shared_from_this(), Vehicle_request::Result::NOK);
    vehicle_command = Vehicle_command_request::Create(completion_handler,
        completion_context, message->payload);
    vehicle->Submit_vehicle_request(vehicle_command);
}

void
Ucs_vehicle_command_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::ADSB_TRANSPONDER_PREFLIGHT,
                         mavlink::ugcs::Extension>::Ptr message)
{
    if (vehicle_command) {
        /* Already in progress. */
        Send_adsb_ack(mavlink::MAV_RESULT::MAV_RESULT_TEMPORARILY_REJECTED);
        return;
    }

    /* Start new transaction. */
    auto completion_handler =
            Make_callback(&Ucs_vehicle_command_transaction::On_adsb_command_completed,
                    Shared_from_this(), Vehicle_request::Result::NOK);
    vehicle_command = Vehicle_command_request::Create(completion_handler,
        completion_context, message->payload);
    vehicle->Submit_vehicle_request(vehicle_command);
}

void
Ucs_vehicle_command_transaction::On_vehicle_command_completed(Vehicle_request::Result result)
{
    LOG_DEBUG("Command completed for vehicle %u, result %d",
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

void
Ucs_vehicle_command_transaction::On_adsb_command_completed(Vehicle_request::Result result)
{
    LOG_DEBUG("Command completed for vehicle %u, result %d",
            vehicle->system_id,
            static_cast<int>(result));

    if (!Is_aborted()) {
        Send_adsb_ack(result == Vehicle_request::Result::OK ?
                mavlink::MAV_RESULT::MAV_RESULT_ACCEPTED :
                mavlink::MAV_RESULT::MAV_RESULT_FAILED);
    }
    vehicle_command = nullptr;
    Done();
}

