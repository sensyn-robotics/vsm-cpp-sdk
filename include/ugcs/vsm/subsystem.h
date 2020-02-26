// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _UGCS_VSM_SUBSYSTEM_H_
#define _UGCS_VSM_SUBSYSTEM_H_

#include <ugcs/vsm/property.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ugcs {
namespace vsm {

uint32_t
Get_unique_id();

typedef std::shared_ptr<proto::Vsm_message> Proto_msg_ptr;

class Vsm_command: public std::enable_shared_from_this<Vsm_command>
{
    DEFINE_COMMON_CLASS(Vsm_command, Vsm_command)

public:
    Vsm_command(std::string name, bool in_mission);

    Property::Ptr
    Add_parameter(
        std::string,
        proto::Field_semantic semantic = proto::FIELD_SEMANTIC_DEFAULT);

    Property::Ptr
    Add_parameter(std::string name, Property::Value_type type);

    // Fill registration message
    void
    Register(proto::Register_command* command);

    // Fill availability message
    void
    Set_capabilities(proto::Command_availability* msg);

    void
    Set_enabled(bool is_enabled = true);

    void
    Set_available(bool is_available = true);

    int
    Get_id() {return command_id;}

    bool
    Is_capability_state_dirty() {return capability_state_dirty;}

    // create a list of command param values from proto message
    Property_list
    Build_parameter_list(const proto::Device_command &cmd);

    //Create command message with given arguments
    void
    Build_command(proto::Device_command *cmd, Property_list args = Property_list());

    std::string
    Get_name()
    {return name;}

    bool
    Is_mission_item()
        {return in_mission;}

private:
    uint32_t command_id = 0;
    std::unordered_map<int, Property::Ptr> parameters;
    std::string name;

    bool in_mission = false;

    bool is_available = false;
    bool is_enabled = false;
    bool capability_state_dirty = true;
    friend class Device;
};

class Subsystem: public std::enable_shared_from_this<Subsystem>
{
    DEFINE_COMMON_CLASS(Subsystem, Subsystem)

public:
    Subsystem(proto::Subsystem_type);

    /** Disable copying. */
    Subsystem(const Subsystem &) = delete;

    // Create/replace property value. By default type is derived from value.
    template<typename Type>
    Property::Ptr
    Set_property(
        const std::string& name,
        Type value,
        proto::Field_semantic semantic = proto::FIELD_SEMANTIC_DEFAULT)
    {
        auto it = properties.find(name);
        if (it == properties.end()) {
            auto f = Property::Create(name, value, semantic);
            properties.emplace(name, f);
            return f;
        } else {
            it->second->Set_value(value);
            return it->second;
        }
    }

    Property::Ptr
    Add_telemetry(
        const std::string& name,
        proto::Field_semantic sem = proto::FIELD_SEMANTIC_DEFAULT,
        uint32_t timeout = 0);   // timeout in seconds. default: do not specify timeout

    Property::Ptr
    Add_telemetry(
        const std::string& name,
        Property::Value_type type,
        uint32_t timeout = 0);   // timeout in seconds. default: do not specify timeout

    // Remove previously added telemetry field.
    // Invalidates the t_field.
    void
    Remove_telemetry(Property::Ptr& t_field);

    Vsm_command::Ptr
    Add_command(
        const std::string& name,
        bool in_mission);

    // Populate subsystem registration message
    void
    Register(proto::Register_subsystem* msg);

    proto::Subsystem_type type;

    std::unordered_map<std::string, Property::Ptr> properties;
    // All registered telemetry fields.
    std::vector<Property::Ptr> telemetry_fields;
    // All commands supported by device
    std::unordered_map<int, Vsm_command::Ptr> commands;

    std::vector<Subsystem::Ptr> subsystems;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_SUBSYSTEM_H_ */
