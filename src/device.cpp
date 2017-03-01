/*
 * device.cpp
 *
 *  Created on: May 5, 2016
 *      Author: janis
 */

#include <ugcs/vsm/device.h>
#include <ugcs/vsm/cucs_processor.h>
#include <ugcs/vsm/actions.h>

using namespace ugcs::vsm;

namespace
{

static std::atomic<uint32_t> current_unique_id(1);


uint32_t
Get_unique_id()
{
    return current_unique_id++;
}
}

Vsm_command::Vsm_command(std::string name, bool as_command, bool in_mission):
    command_id(Get_unique_id()),
    name(name), as_command(as_command), in_mission(in_mission)
{
}

Property::Ptr
Vsm_command::Add_parameter(
    std::string name,
    ugcs::vsm::proto::Field_semantic semantic)
{
    auto p = Property::Create(Get_unique_id(), name, semantic);
    parameters.emplace(p->Get_id(), p);
    return p;
}

Property::Ptr
Vsm_command::Add_parameter(
    std::string name,
    Property::Value_type type)
{
    auto p = Property::Create(Get_unique_id(), name, type);
    parameters.emplace(p->Get_id(), p);
    return p;
}

void
Vsm_command::Set_capabilities(ugcs::vsm::proto::Command_availability* msg)
{
    msg->set_id(command_id);
    msg->set_is_available(is_available);
    msg->set_is_enabled(is_enabled);
    capability_state_dirty = false;
}

void
Vsm_command::Register(ugcs::vsm::proto::Register_command* msg)
{
    msg->set_name(name);
    msg->set_id(command_id);
    msg->set_available_as_command(as_command);
    msg->set_available_in_mission(in_mission);
    for (auto it: parameters) {
        it.second->Register(msg->add_parameters());
    }
}

void
Vsm_command::Set_enabled(bool value)
{
    if (is_enabled != value) {
        capability_state_dirty = true;
        is_enabled = value;
    }
}

void
Vsm_command::Set_available(bool value)
{
    if (is_available != value) {
        capability_state_dirty = true;
        is_available = value;
    }
}

Property_list
Vsm_command::Build_parameter_list(const ugcs::vsm::proto::Device_command &cmd)
{
    std::unordered_map<std::string, Property::Ptr> ret;
    for (int i = 0; i < cmd.parameters_size(); i++) {
        auto fid = cmd.parameters(i).field_id();
        auto pit = parameters.find(fid);
        if (pit == parameters.end()) {
            LOG_ERR("Unknown parameter %d for command %d", fid, cmd.command_id());
            ret.clear();
            return ret;
        }
        auto param = pit->second;
        // Create copy of command parameter.
        auto prop = Property::Create(param);
        if (prop->Set_value(cmd.parameters(i).value())) {
            ret.emplace(param->Get_name(), prop);
            LOG("%s", prop->Dump_value().c_str());
        } else {
            LOG_ERR("Invalid parameter %s value for command %s", param->Get_name().c_str(), name.c_str());
            ret.clear();
            return ret;
        }
    }
    return ret;
}

Ucs_request::Ucs_request(ugcs::vsm::proto::Vsm_message m):
    request(std::move(m))
{

}

void
Ucs_request::Complete(ugcs::vsm::proto::Status_code s, const std::string& description)
{
    if (!Is_completed()) {
        if (response) {
            response->mutable_device_response()->set_code(s);
            if (description.size()) {
                response->mutable_device_response()->set_status(description);
            }
        }
        Request::Complete();
    }
}

Device::Device(bool create_thread)
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
    begin_of_epoch = std::chrono::system_clock::now();
}

Device::~Device()
{
}

uint32_t
Device::Get_session_id()
{
    return my_handle;
}

void
Device::Enable()
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
Device::Disable()
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
    completion_ctx = nullptr;
    processor = nullptr;
    worker = nullptr;
}

/** Get default completion context of the vehicle. */
Request_completion_context::Ptr
Device::Get_completion_ctx()
{
    return completion_ctx;
}

Request_processor::Ptr
Device::Get_processing_ctx()
{
    return processor;
}

void
Device::Process_requests()
{
    ASSERT(!worker);
    completion_ctx->Process_requests();
    processor->Process_requests();
}

void
Device::Register()
{
    if (!my_handle) {
        CREATE_COMMIT_SCOPE;
        my_handle = Get_unique_id();
        Cucs_processor::Get_instance()->Register_device(Shared_from_this());
        // Push pending telemetry if any.
    }
}

void
Device::Unregister()
{
    if (my_handle) {
        Cucs_processor::Get_instance()->Unregister_device(my_handle);
        my_handle = 0;
    }
}

void
Device::On_ucs_message(
    ugcs::vsm::proto::Vsm_message message,
    Response_sender completion_handler,
    ugcs::vsm::Request_completion_context::Ptr completion_ctx)
{
    Proto_msg_ptr response = nullptr;

    auto request = Ucs_request::Create(std::move(message));

    if (completion_ctx) {
        response = completion_handler.Get_arg<1>();
        request->Set_completion_handler(completion_ctx, completion_handler);
        request->response = response;
    }

    request->Set_processing_handler(
        Make_callback(
            &Device::Handle_ucs_command,
            Shared_from_this(),
            request
            ));

    processor->Submit_request(request);
}

void
Device::Send_ucs_message(Proto_msg_ptr msg)
{
    if (my_handle) {
        Cucs_processor::Get_instance()->Send_ucs_message(my_handle, msg);
    } else {
        LOG_ERR("Send while device not registered");
    }
}

void
Device::Handle_ucs_command(
    Ucs_request::Ptr ucs_request)
{
    ucs_request->Complete(ugcs::vsm::proto::STATUS_FAILED, "Not implemented");
}

Vsm_command::Ptr
Device::Add_command(
    const std::string& name,
    bool available_as_command,
    bool available_in_mission)
{
    auto c = Vsm_command::Create(name, available_as_command, available_in_mission);
    commands.emplace(c->Get_id(), c);
    return c;
}

Property::Ptr
Device::Add_telemetry(
    const std::string& name,
    ugcs::vsm::proto::Field_semantic semantic,
    uint32_t timeout)
{
    auto t = Property::Create(Get_unique_id(), name, semantic);
    if (timeout) {
        t->Set_timeout(timeout);
    }
    telemetry_fields.push_back(t);
    return t;
}

Property::Ptr
Device::Add_telemetry(
    const std::string& name,
    ugcs::vsm::Property::Value_type type,
    uint32_t timeout)
{
    auto t = Property::Create(Get_unique_id(), name, type);
    if (timeout) {
        t->Set_timeout(timeout);
    }
    telemetry_fields.push_back(t);
    return t;
}

void
Device::Remove_telemetry(Property::Ptr& p)
{
    for (auto f = telemetry_fields.begin(); f != telemetry_fields.end(); f++) {
        if ((*f)->Get_id() == p->Get_id()) {
            telemetry_fields.erase(f);
            p = nullptr;
            return;
        }
    }
}

void
Device::Commit_to_ucs()
{
    if (!my_handle) {
        // Do not send any telemetry if not registered thus
        // Avoiding resetting dirty flags on command avail and telemetry.
        return;
    }
    auto msg = std::make_shared<ugcs::vsm::proto::Vsm_message>();
    auto report = msg->mutable_device_status();
    for (auto it : telemetry_fields) {
        if (it->Is_changed()) {
            auto tf = report->add_telemetry_fields();
            it->Write_as_telemetry(tf);
            tf->set_ms_since_epoch(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    it->Get_update_time() - begin_of_epoch).count());
        }
    }

    for (auto it : commands) {
        if (it.second->Is_capability_state_dirty()) {
            it.second->Set_capabilities(report->add_command_availability());
        }
    }

    while (!device_status_messages.empty()) {
        report->add_status_messages(device_status_messages.front());
        device_status_messages.pop_front();
    }

    if (    report->telemetry_fields_size()
        ||  report->command_availability_size()
        ||  report->status_messages_size())
    {
        Send_ucs_message(msg);
    }
}

Vsm_command::Ptr
Device::Get_command(int id)
{
    auto cit = commands.find(id);
    if (cit == commands.end()) {
        return nullptr;
    } else {
        return cit->second;
    }
}

void
Device::Add_status_message(const std::string& m)
{
    device_status_messages.push_back(m);
}
