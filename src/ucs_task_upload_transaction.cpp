// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/ucs_task_upload_transaction.h>
#include <ugcs/vsm/properties.h>
#include <ugcs/vsm/ucs_to_vsm_transformations.h>

using namespace ugcs::vsm;

std::string
Ucs_task_upload_transaction::Get_name() const
{
    return "TASK_UPLOAD";
}

void
Ucs_task_upload_transaction::On_abort()
{
    if (!awaiting_vehicle) {
        if (task) {
            task->Abort();
            task = nullptr;
        }
        Done();
    }
}

void
Ucs_task_upload_transaction::On_enable()
{

}

void
Ucs_task_upload_transaction::On_disable()
{
    if (task) {
        task->Abort();
        task = nullptr;
    }
}

/* XXX This method should be implemented based on official UCS/VSM protocol
 * which is still under development.
 */
bool
Ucs_task_upload_transaction::Verify_task()
{
    if (takeoff_altitude) {
        task->payload.Set_takeoff_altitude(*takeoff_altitude);
        VEHICLE_LOG_DBG(*vehicle, "Take-off altitude is %.2f meters.", *takeoff_altitude);
    } else {
        return false;
    }
    for (auto& iter: task->payload.actions) {
        if (iter->Get_type() == Action::Type::MOVE ||
            iter->Get_type() == Action::Type::TAKEOFF) {
            return true;
        }
    }

    return false;
}

void
Ucs_task_upload_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_COUNT_EX,
                         mavlink::ugcs::Extension>::Ptr message)
{
    mavlink::MAV_MISSION_RESULT result =
            mavlink::MAV_MISSION_RESULT::MAV_MISSION_ACCEPTED;
    bool done = false;
    if (task) {
        /* Already in progress. */
        result = mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID_SEQUENCE;
    }
    if (!task && !message->payload->count) {
        /* No actions, UCS error. */
        done = true;
        result = mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID;
    }
    if (result != mavlink::MAV_MISSION_RESULT::MAV_MISSION_ACCEPTED) {
        Send_mission_ack(result, message);
    }
    if (done) {
        Done();
    }
    if (result != mavlink::MAV_MISSION_RESULT::MAV_MISSION_ACCEPTED || done) {
        return;
    }

    /* New transaction. */
    vehicle_component_id = message->payload->target_component;
    auto completion_handler =
            Make_callback(&Ucs_task_upload_transaction::On_mission_upload_completed,
                    Shared_from_this(), Vehicle_request::Result::NOK);
    task = Vehicle_task_request::Create(completion_handler, completion_context,
            message->payload->count);
    task_item_count = message->payload->count;
    takeoff_altitude.Disengage();
    Get_next_mission_item();
}

void Ucs_task_upload_transaction::Process(
        mavlink::Message<mavlink::ugcs::MESSAGE_ID::MISSION_ITEM_EX,
                         mavlink::ugcs::Extension>::Ptr message)
{
    /* Check for unexpected message. */
    if (awaiting_vehicle || !task ||
	   (vehicle_component_id != message->payload->target_component) ||
	   (message->payload->seq != current_task_item)) {
        Send_mission_ack(
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_INVALID_SEQUENCE,
                message);
        Done();
        return;
    }

    LOG_DEBUG("Mission item %d for vehicle [%s:%s] received: %s",
            message->payload->seq.Get(), vehicle->Get_model_name().c_str(),
            vehicle->Get_serial_number().c_str(),
            message->payload.Dump().c_str());
    Action::Ptr action;
    /* Dummy, not used. */
    Optional<double> local_takeoff_altitude;
    /* Fill by first occurrence. */
    Optional<double>& takeoff_altitude_ref =
        takeoff_altitude ? local_takeoff_altitude : takeoff_altitude;
    try {
        action = Ucs_to_vsm_transformations::Parse_mission_item_ex(
                message->payload, takeoff_altitude_ref);
    } catch (const Ucs_to_vsm_transformations::Unsupported_exception& ex) {
        LOG_WARN("Mission item transform failed: %s", ex.what());
    } catch (const Action::Format_exception& ex) {
        LOG_WARN("Mission item wrong format: %s", ex.what());
    }
    if (!action) {
        Send_mission_ack(mavlink::MAV_MISSION_RESULT::MAV_MISSION_UNSUPPORTED,
                vehicle_component_id);
        Done();
    } else {
        if (action->Get_type() == Action::Type::TASK_ATTRIBUTES) {
            task->payload.attributes =
                    action->Get_action<Action::Type::TASK_ATTRIBUTES>();
        } else {
            task->payload.actions.push_back(action);
        }
        current_task_item++;
        Get_next_mission_item();
    }
}

void
Ucs_task_upload_transaction::Get_next_mission_item()
{
    if (current_task_item >= task_item_count) {
        /* Done. */
        ASSERT(current_task_item == task_item_count);
        LOG_INFO("All mission items received for vehicle [%s:%s]",
                vehicle->Get_model_name().c_str(),
                vehicle->Get_serial_number().c_str());
        if (Verify_task()) {
            vehicle->Submit_vehicle_request(task);
            awaiting_vehicle = true;
        } else {
            LOG_ERROR("Task received for vehicle [%s:%s] is not valid.",
                vehicle->Get_model_name().c_str(),
                vehicle->Get_serial_number().c_str());
            Send_mission_ack(mavlink::MAV_MISSION_RESULT::MAV_MISSION_ERROR,
                    vehicle_component_id);
            Done();
        }
    } else {
        LOG_DEBUG("Requesting mission item %zu.", current_task_item);

        mavlink::Pld_mission_request req;
        req->seq = current_task_item;
        req->target_system = Get_ucs_system_id();
        req->target_component = Get_ucs_component_id();
        Send_message(req, vehicle->system_id, vehicle_component_id);
    }
}

void
Ucs_task_upload_transaction::On_mission_upload_completed(Vehicle_request::Result result)
{
    ASSERT(awaiting_vehicle);
    LOG_INFO("Mission uploaded to vehicle [%s:%s], result %d",
            vehicle->Get_model_name().c_str(),
            vehicle->Get_serial_number().c_str(),
            static_cast<int>(result));
    if (!Is_aborted()) {
        Send_mission_ack(result == Vehicle_request::Result::OK ?
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_ACCEPTED :
                mavlink::MAV_MISSION_RESULT::MAV_MISSION_ERROR,
                vehicle_component_id);
    }
    task = nullptr;
    Done();
}
