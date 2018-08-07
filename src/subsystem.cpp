// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/subsystem.h>
#include <atomic>

using namespace ugcs::vsm;

static std::atomic<uint32_t> current_unique_id(1);

uint32_t
ugcs::vsm::Get_unique_id()
{
    return current_unique_id++;
}

Vsm_command::Vsm_command(std::string name, bool in_mission):
    command_id(Get_unique_id()),
    name(name), in_mission(in_mission)
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
    msg->set_available_in_mission(in_mission);
    for (auto it : parameters) {
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
    Property_list ret;
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
        } else {
            LOG_ERR("Invalid parameter %s value for command %s", param->Get_name().c_str(), name.c_str());
            ret.clear();
            return ret;
        }
    }
    return ret;
}

Subsystem::Subsystem(proto::Subsystem_type type):
    type(type)
{

}

Vsm_command::Ptr
Subsystem::Add_command(
    const std::string& name,
    bool available_in_mission)
{
    auto c = Vsm_command::Create(name, available_in_mission);
    commands.emplace(c->Get_id(), c);
    return c;
}

Property::Ptr
Subsystem::Add_telemetry(
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
Subsystem::Add_telemetry(
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
Subsystem::Remove_telemetry(Property::Ptr& p)
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
Subsystem::Register(proto::Register_subsystem* msg)
{
    msg->set_type(type);
    for (auto it : properties) {
        it.second->Write_as_property(msg->add_properties());
    }
    for (auto it : commands) {
        it.second->Register(msg->add_commands());
    }
    for (auto it : telemetry_fields) {
        it->Register(msg->add_telemetry_fields());
    }
}
