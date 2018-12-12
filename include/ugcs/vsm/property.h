// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _UGCS_VSM_PROPERTY_H_
#define _UGCS_VSM_PROPERTY_H_

#include <ucs_vsm_proto.h>
#include <ugcs/vsm/utils.h>
#include <ugcs/vsm/optional.h>
#include <unordered_map>

namespace ugcs {
namespace vsm {

class Property: public std::enable_shared_from_this<Property>
{
    DEFINE_COMMON_CLASS(Property, Property)

public:
    typedef enum {
        VALUE_TYPE_INT = 1,
        VALUE_TYPE_FLOAT = 2,
        VALUE_TYPE_DOUBLE = 3,
        VALUE_TYPE_STRING = 4,
        VALUE_TYPE_BOOL = 5,
        VALUE_TYPE_LIST = 6,
        VALUE_TYPE_ENUM = 7,
        VALUE_TYPE_NONE = 8,
    } Value_type;

    typedef enum {
        VALUE_SPEC_REGULAR = 1, // Value is valid
        VALUE_SPEC_NA = 2,      // Value is "N/A"
    } Value_spec;

    // Create from built in semantics
    Property(int id, const std::string& name, proto::Field_semantic semantic);

    // Create with user defined semantic
    Property(int id, const std::string& name, Value_type type);

    // Create a copy of property
    Property(Property::Ptr);

    // Create and set value. Derive type from value if semantic is not given.
    // Throws if
    template<typename Type>
    Property(const std::string& name, Type value, proto::Field_semantic sem = proto::FIELD_SEMANTIC_DEFAULT):
        semantic(sem),
        name(name)
    {
        // First make sure that value of valid type has been passed to us (at compile time)
        static_assert(
            std::is_same<Type, bool>::value ||
            std::is_same<Type, int>::value ||
            std::is_enum<Type>::value ||
            std::is_same<Type, unsigned int>::value ||
            std::is_same<Type, const char*>::value ||
            std::is_same<Type, std::string>::value ||
            std::is_floating_point<Type>::value,
            "Unsupported value type");

        // Secondly, try to derive semantic from property name
        if (semantic == proto::FIELD_SEMANTIC_DEFAULT) {
            semantic = Get_default_semantic(name);
        }

        if (semantic == proto::FIELD_SEMANTIC_DEFAULT) {
            // Semantic is not given then derive property type from value type.
            if (std::is_same<Type, bool>::value) {
                type = VALUE_TYPE_BOOL;
                semantic = proto::FIELD_SEMANTIC_BOOL;
            } else if (std::is_integral<Type>::value) {
                type = VALUE_TYPE_INT;
                semantic = proto::FIELD_SEMANTIC_NUMERIC;
            } else if (std::is_enum<Type>::value) {
                type = VALUE_TYPE_ENUM;
                semantic = proto::FIELD_SEMANTIC_ENUM;
            } else if (std::is_same<Type, float>::value) {
                type = VALUE_TYPE_FLOAT;
                semantic = proto::FIELD_SEMANTIC_NUMERIC;
            } else if (std::is_floating_point<Type>::value) {
                type = VALUE_TYPE_DOUBLE;
                semantic = proto::FIELD_SEMANTIC_NUMERIC;
            } else if (std::is_same<Type, const char*>::value || std::is_same<Type, std::string>::value) {
                type = VALUE_TYPE_STRING;
                semantic = proto::FIELD_SEMANTIC_STRING;
            }
        } else {
            type = Get_type_from_semantic(semantic);
        }
        Set_value(value);
    }

    void
    Add_enum(const std::string& name, int value);

    void
    Set_value(bool v);

    void
    Set_value(double v);

    void
    Set_value(const std::string& v);

    void
    Set_value(const char* v);

    void
    Set_value(int v);

    void
    Set_value(unsigned int v);

    void
    Set_value(const proto::List_value &v);

    void
    Set_value_na();

    // Set value from incoming message
    bool
    Set_value(const proto::Field_value& val);

    void
    Register(proto::Register_field* field);

    void
    Write_as_property(proto::Property_field* field);

    void
    Write_as_parameter(proto::Parameter_field* field);

    void
    Write_as_telemetry(proto::Telemetry_field* field);

    void
    Set_timeout(int);

    Property::Ptr
    Min_value();

    Property::Ptr
    Max_value();

    Property::Ptr
    Default_value();

    bool
    Get_value(bool &v); // NOLINT(runtime/references)

    bool
    Get_value(float &v); // NOLINT(runtime/references)

    bool
    Get_value(double &v); // NOLINT(runtime/references)

    bool
    Get_value(std::string& v); // NOLINT(runtime/references)

    bool
    Get_value(int &v); // NOLINT(runtime/references)

    bool
    Get_value(int64_t &v); // NOLINT(runtime/references)

    bool
    Get_value(proto::List_value &v); // NOLINT(runtime/references)

    bool
    Is_value_na();

    // true if value has been changed.
    // Checks for timeout and automatically sets value to NA and
    // returns true if value has timeouted.
    bool
    Is_changed();

    // Force sending telemetry on value update even if value has not changed.
    void
    Set_changed();

    int
    Get_id() {return field_id;}

    std::string
    Get_name() {return name;}

    proto::Field_semantic
    Get_semantic() {return semantic;}

    std::chrono::time_point<std::chrono::system_clock>
    Get_update_time() {return update_time;}

    std::string
    Dump_value();

    bool
    Fields_are_equal(const proto::Field_value& val1, const proto::Field_value& val2);

    bool
    Is_equal(const Property&);

private:
    void
    Write_value(proto::Field_value* field);

    bool is_changed = false; // new value set. reset to false after sending to ucs.

    Value_type type = VALUE_TYPE_NONE;
    proto::Field_semantic semantic = proto::FIELD_SEMANTIC_DEFAULT;

    int field_id = 0;
    std::string name;

    std::string string_value;
    bool bool_value = false;
    double double_value = 0;
    int64_t int_value = 0;
    proto::List_value list_value;


    Property::Ptr default_value;
    Property::Ptr min_value;
    Property::Ptr max_value;

    std::unordered_map<int, std::string> enum_values;
    std::chrono::seconds timeout = std::chrono::seconds(0); // timeout in seconds
    Value_spec value_spec = VALUE_SPEC_NA;
    std::chrono::time_point<std::chrono::system_clock> update_time;

    // time when field was last sent to server.
    // Used to throttle telemetry sending to server.
    std::chrono::time_point<std::chrono::steady_clock> last_commit_time;

    // do not send telemetry field to server more than 5 times per second.
    // TODO: make this configurable
    static constexpr std::chrono::milliseconds COMMIT_TIMEOUT = std::chrono::milliseconds(200);

    static proto::Field_semantic
    Get_default_semantic(const std::string& name);

    static Value_type
    Get_type_from_semantic(const proto::Field_semantic sem);
};

class Property_list : public std::unordered_map<std::string, Property::Ptr>
{
public:
    template<typename Type>
    bool
    Get_value(const std::string& name, Type& value) const // NOLINT(runtime/references)
    {
        auto it = find(name);
        if (it != end() && !it->second->Is_value_na()) {
            return it->second->Get_value(value);
        }
        return false;
    }

    // true if property all values are the same in both lists.
    bool
    Is_equal(const Property_list&);
};


} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_PROPERTY_H_ */
