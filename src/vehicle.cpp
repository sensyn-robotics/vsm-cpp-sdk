// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Vehicle class implementation.
 */

#include <ugcs/vsm/vehicle.h>
#include <ugcs/vsm/log.h>
#include <ugcs/vsm/cucs_processor.h>

#include <iostream>

using namespace ugcs::vsm;

std::hash<Vehicle*> Vehicle::Hasher::hasher;

std::mutex Vehicle::state_mutex;

Vehicle::Vehicle(mavlink::MAV_TYPE type, mavlink::MAV_AUTOPILOT autopilot,
        const Capabilities& capabilities,
        const std::string& serial_number, const std::string& model_name,
        bool create_thread) :
                serial_number(serial_number),
                model_name(model_name),
                type(type),
                autopilot(autopilot),
                capabilities(capabilities),
                reset_altitude_origin(false)
{
    completion_ctx = Request_completion_context::Create("Vehicle completion");
    processor = Request_processor::Create("Vehicle processor");
    if (create_thread) {
        worker = Request_worker::Create(
                "Vehicle worker",
                std::initializer_list<Request_container::Ptr>(
                        {completion_ctx, processor}));
    } else {
        worker = nullptr;
    }
}

void
Vehicle::Enable()
{
    ASSERT(!is_enabled);
    completion_ctx->Enable();
    processor->Enable();
    if (worker) {
        worker->Enable();
    }
    is_enabled = true;
    On_enable();
    Register();
}

void
Vehicle::Disable()
{
    Unregister();
    ASSERT(is_enabled);
    is_enabled = false;
    On_disable();
    completion_ctx->Disable();
    processor->Disable();
    if (worker) {
        worker->Disable();
    }
    telemetry.Unregister_transport_interface();
    completion_ctx = nullptr;
    processor = nullptr;
    worker = nullptr;
}

bool
Vehicle::Is_enabled()
{
    return is_enabled;
}

void
Vehicle::Register()
{
    Calculate_system_id();
    Cucs_processor::Get_instance()->Register_vehicle(Shared_from_this());
}

void
Vehicle::Unregister()
{
    Cucs_processor::Get_instance()->Unregister_vehicle(Shared_from_this());
}

Request_processor::Ptr
Vehicle::Get_processing_ctx()
{
    return processor;
}

void
Vehicle::Process_requests()
{
    ASSERT(!worker);
    completion_ctx->Process_requests();
    processor->Process_requests();
}

void
Vehicle::Handle_vehicle_request(Vehicle_task_request::Handle)
{
    LOG_DEBUG("Mission to vehicle [%s:%s] is ignored.", 
    Get_serial_number().c_str(), Get_model_name().c_str());
}

void
Vehicle::Handle_vehicle_request(Vehicle_clear_all_missions_request::Handle)
{
    LOG_DEBUG("Clear all missions for vehicle [%s:%s] is ignored.", Get_serial_number().c_str(),
            Get_model_name().c_str());
}

void
Vehicle::Handle_vehicle_request(Vehicle_command_request::Handle)
{
    LOG_DEBUG("Command for vehicle [%s:%s] is ignored.", Get_serial_number().c_str(),
            Get_model_name().c_str());
}

const std::string&
Vehicle::Get_serial_number() const
{
    return serial_number;
}

const std::string&
Vehicle::Get_model_name() const
{
    return model_name;
}

ugcs::vsm::mavlink::MAV_AUTOPILOT
Vehicle::Get_autopilot() const
{
    return autopilot;
}

Request_completion_context::Ptr
Vehicle::Get_completion_ctx()
{
    return completion_ctx;
}

Vehicle::Sys_status::Sys_status(
        bool uplink_connected,
        bool downlink_connected,
        Control_mode control_mode,
        State state,
        std::chrono::seconds uptime) :
        uplink_connected(uplink_connected), downlink_connected(downlink_connected),
        control_mode(control_mode), state(state), uptime(uptime)
{

}

bool
Vehicle::Sys_status::operator==(const Sys_status& other) const
{
    return uplink_connected == other.uplink_connected &&
           downlink_connected == other.downlink_connected &&
           state == other.state &&
           control_mode == other.control_mode;
}

void
Vehicle::Set_system_status(const Sys_status& system_status)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(system_status == this->sys_status);
        this->sys_status = system_status;
    }
    if (update) {
        telemetry.Sys_status_update();
    }
}

Vehicle::Sys_status
Vehicle::Get_system_status() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return sys_status;
}

Vehicle::Capabilities
Vehicle::Get_capabilities() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return capabilities;
}

void
Vehicle::Set_capabilities(const Capabilities& capabilities)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(this->capabilities == capabilities);
        this->capabilities = capabilities;
    }
    if (update) {
        telemetry.Sys_status_update();
    }
}

void
Vehicle::Reset_altitude_origin()
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(this->reset_altitude_origin);
        this->reset_altitude_origin = true;
    }
    if (update) {
        telemetry.Sys_status_update();
    }
}

bool
Vehicle::Read_reset_altitude_origin()
{
    std::unique_lock<std::mutex> lock(state_mutex);
    if (this->reset_altitude_origin) {
        this->reset_altitude_origin = false;
        return true;
    } else {
        return false;
    }
}

Vehicle::Capability_states
Vehicle::Get_capability_states() const
{
    std::unique_lock<std::mutex> lock(state_mutex);
    return capability_states;
}

void
Vehicle::Set_capability_states(const Capability_states& capability_states)
{
    bool update;
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        update = !(this->capability_states == capability_states);
        this->capability_states = capability_states;
    }
    if (update) {
        telemetry.Sys_status_update();
    }
}

Telemetry_manager::Report::Ptr
Vehicle::Open_telemetry_report()
{
    auto status = Get_system_status();
    return telemetry.Open_report(status.uptime);
}

namespace {

uint64_t
fnv1a_hash64(const uint8_t *x, size_t n)
{
    uint64_t v = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) {
        v ^= x[i];
        v *= 0x100000001b3ULL;
    }
    return v;
}

} /* anonymous namespace */

/*
 * WARNING:
 * When changing this function, make sure to sync it with
 * VehicleIdentity::getVehicleId() in vsm-java-sdk.
 */
void
Vehicle::Calculate_system_id()
{
    if (serial_number.empty()) {
        VSM_EXCEPTION(Exception, "Serial number should be set before ID is calculated");
    }
    if (model_name.empty()) {
        VSM_EXCEPTION(Exception, "Model name should be set before ID is calculated");
    }
    auto seed = serial_number + model_name;
    const char* buff = seed.c_str();
    auto h = fnv1a_hash64(reinterpret_cast<const uint8_t*>(buff), seed.length());
    system_id = h;
}

void
Vehicle::Register_telemetry_interface(Telemetry_interface& telemetry)
{
    this->telemetry.Register_transport_interface(telemetry);
}

void
Vehicle::Unregister_telemetry_interface()
{
    this->telemetry.Unregister_transport_interface();
}
