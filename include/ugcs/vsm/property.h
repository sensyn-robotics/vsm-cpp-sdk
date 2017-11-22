/*
 * property.h
 *
 *  Created on: Jul 10, 2016
 *      Author: janis
 */

#ifndef SRC_PROPERTY_H_
#define SRC_PROPERTY_H_

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
    Property(int id, const std::string& name, ugcs::vsm::proto::Field_semantic semantic);

    // Create with user defined semantic
    Property(int id, const std::string& name, Value_type type);

    // Create a copy of property
    Property(Property::Ptr);

    // Create and set value.
    template<typename Type>
    Property(
        const std::string& name,
        Type value,
        Value_type value_type):Property(0, name, value_type)
    {
        Set_value(value);
    }

    // Create and set value.
    template<typename Type>
    Property(
        const std::string& name,
        ugcs::vsm::proto::Field_semantic semantic,
        Type value):Property(0, name, semantic)
    {
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
    Set_value(ugcs::vsm::proto::List_value &v);

    void
    Set_value_na();

    // Set value from incoming message
    bool
    Set_value(const ugcs::vsm::proto::Field_value& val);

    void
    Register(ugcs::vsm::proto::Register_field* field);

    void
    Write_as_property(ugcs::vsm::proto::Property_field* field);

    void
    Write_as_parameter(ugcs::vsm::proto::Parameter_field* field);

    void
    Write_as_telemetry(ugcs::vsm::proto::Telemetry_field* field);

    void
    Set_timeout(int);

    Property::Ptr
    Min_value();

    Property::Ptr
    Max_value();

    Property::Ptr
    Default_value();

    bool
    Get_value(bool &v);

    bool
    Get_value(float &v);

    bool
    Get_value(double &v);

    bool
    Get_value(std::string& v);

    bool
    Get_value(int &v);

    bool
    Get_value(ugcs::vsm::proto::List_value &v);

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
    Get_id() {return field_id;};

    std::string
    Get_name() {return name;};

    ugcs::vsm::proto::Field_semantic
    Get_semantic() {return semantic;};

    std::chrono::time_point<std::chrono::system_clock>
    Get_update_time() {return update_time;};

    std::string
    Dump_value();

    bool
    Fields_are_equal(const ugcs::vsm::proto::Field_value& val1, const ugcs::vsm::proto::Field_value& val2);

private:

    void
    Write_value(ugcs::vsm::proto::Field_value* field);

    bool is_changed = false; // new value set. reset to false after sending to ucs.

    Value_type type = VALUE_TYPE_NONE;
    ugcs::vsm::proto::Field_semantic semantic = ugcs::vsm::proto::FIELD_SEMANTIC_DEFAULT;

    int field_id = 0;
    std::string name;

    std::string string_value;
    bool bool_value = false;
    double double_value = 0;
    int int_value = 0;
    ugcs::vsm::proto::List_value list_value;


    Property::Ptr default_value;
    Property::Ptr min_value;
    Property::Ptr max_value;

    std::unordered_map<int, std::string> enum_values;
    std::chrono::seconds timeout = std::chrono::seconds(0); // timeout in seconds
    Value_spec value_spec = VALUE_SPEC_NA;
    std::chrono::time_point<std::chrono::system_clock> update_time;
};

class Property_list : public std::unordered_map<std::string, Property::Ptr>
{
public:
    template<typename Type>
    bool
    Get_value(const std::string& name, Type& value)
    {
        auto it = find(name);
        if (it != end() && !it->second->Is_value_na()) {
            return it->second->Get_value(value);
        }
        return false;
    }
};


} /* namespace vsm */
} /* namespace ugcs */

#endif /* SRC_PROPERTY_H_ */
