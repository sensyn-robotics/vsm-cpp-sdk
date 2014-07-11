// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/ucs_mission_clear_all_transaction.h>

using namespace ugcs::vsm;

std::string
Ucs_mission_clear_all_transaction::Get_name() const
{
    return "CLEAR_ALL_MISSIONS";
}

void
Ucs_mission_clear_all_transaction::On_abort()
{
    if (!clear_all_missions) {
        Done();
    }
}

void
Ucs_mission_clear_all_transaction::On_disable()
{
    if (clear_all_missions) {
        clear_all_missions->Abort();
        clear_all_missions = nullptr;
    }
}

void
Ucs_mission_clear_all_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_CLEAR_ALL_EX,
                         mavlink::ugcs::Extension>::Ptr message)
{
    if (clear_all_missions) {
        /* Already in progress. */
        Send_mission_ack(
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID_SEQUENCE,
                message);
        return;
    }

    /* Start new transaction. */
    vehicle_component_id = message->payload->target_component;
    auto completion_handler =
            Make_callback(&Ucs_mission_clear_all_transaction::On_clear_all_missions_completed,
                    Shared_from_this(), Vehicle_request::Result::NOK);
    clear_all_missions = Vehicle_clear_all_missions_request::Create(completion_handler,
            completion_context);
    vehicle->Submit_vehicle_request(clear_all_missions);
}

void
Ucs_mission_clear_all_transaction::On_clear_all_missions_completed(Vehicle_request::Result result)
{
    LOG_DEBUG("Mission clear all completed for vehicle %u, result %d",
            vehicle->system_id,
            static_cast<int>(result));

    if (!Is_aborted()) {
        Send_mission_ack(result == Vehicle_request::Result::OK ?
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_ACCEPTED :
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_ERROR,
                vehicle_component_id);
    }
    clear_all_missions = nullptr;
    Done();
}
