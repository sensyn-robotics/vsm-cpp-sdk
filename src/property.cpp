// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/property.h>
#include <cmath>

using namespace ugcs::vsm;

constexpr std::chrono::milliseconds Property::COMMIT_TIMEOUT;

proto::Field_semantic
Property::Get_default_semantic(const std::string& name)
{
    if (name == "latitude") {
        return proto::FIELD_SEMANTIC_LATITUDE;
    } else if (name == "longitude") {
        return proto::FIELD_SEMANTIC_LONGITUDE;
    } else if (name == "altitude_amsl") {
        return proto::FIELD_SEMANTIC_ALTITUDE_AMSL;
    } else if (name == "altitude_origin") {
        return proto::FIELD_SEMANTIC_ALTITUDE_AMSL;
    } else if (name == "altitude_raw") {
        return proto::FIELD_SEMANTIC_ALTITUDE_RAW;
    } else if (name == "altitude_agl") {
        return proto::FIELD_SEMANTIC_ALTITUDE_AGL;
    } else if (name == "ground_elevation") {
        return proto::FIELD_SEMANTIC_GROUND_ELEVATION;
    } else if (name == "acceptance_radius") {
        return proto::FIELD_SEMANTIC_ACCEPTANCE_RADIUS;
    } else if (name == "heading") {
        return proto::FIELD_SEMANTIC_HEADING;
    } else if (name == "ms") {
        return proto::FIELD_SEMANTIC_MILLISECONDS;
    } else if (name == "course") {
        return proto::FIELD_SEMANTIC_HEADING;
    } else if (name == "pitch") {
        return proto::FIELD_SEMANTIC_PITCH;
    } else if (name == "yaw") {
        return proto::FIELD_SEMANTIC_HEADING;
    } else if (name == "roll") {
        return proto::FIELD_SEMANTIC_ROLL;
    } else if (name == "ground_speed") {
        return proto::FIELD_SEMANTIC_GROUND_SPEED;
    } else if (name == "air_speed") {
        return proto::FIELD_SEMANTIC_AIR_SPEED;
    } else if (name == "fov_h") {
        return proto::FIELD_SEMANTIC_FOV_H;
    } else if (name == "fov_v") {
        return proto::FIELD_SEMANTIC_FOV_V;
    } else if (name == "main_voltage") {
        return proto::FIELD_SEMANTIC_VOLTAGE;
    } else if (name == "main_current") {
        return proto::FIELD_SEMANTIC_CURRENT;
    } else if (name == "satellite_count") {
        return proto::FIELD_SEMANTIC_SATELLITE_COUNT;
    } else if (name == "gps_fix") {
        return proto::FIELD_SEMANTIC_GPS_FIX_TYPE;
    } else if (name == "gcs_link_quality") {
        return proto::FIELD_SEMANTIC_GCS_LINK_QUALITY;
    } else if (name == "rc_link_quality") {
        return proto::FIELD_SEMANTIC_RC_LINK_QUALITY;
    } else if (name == "control_mode") {
        return proto::FIELD_SEMANTIC_CONTROL_MODE;
    } else if (name == "vertical_speed") {
        return proto::FIELD_SEMANTIC_VERTICAL_SPEED;
    } else if (name == "climb_rate") {
        return proto::FIELD_SEMANTIC_VERTICAL_SPEED;
    } else if (name == "descent_rate") {
        return proto::FIELD_SEMANTIC_VERTICAL_SPEED;
    } else if (name == "flight_mode") {
        return proto::FIELD_SEMANTIC_FLIGHT_MODE;
    } else if (name == "native_flight_mode") {
        return proto::FIELD_SEMANTIC_STRING;
    } else if (name == "autopilot_status") {
        return proto::FIELD_SEMANTIC_AUTOPILOT_STATUS;
    } else if (name == "name") {
        return proto::FIELD_SEMANTIC_STRING;
    } else if (name == "time") {
        return proto::FIELD_SEMANTIC_TIMESTAMP;
    } else if (name == "humidity") {
        return proto::FIELD_SEMANTIC_HUMIDITY;
    } else if (name == "temperature") {
        return proto::FIELD_SEMANTIC_TEMPERATURE;
    } else if (name == "precipitation") {
        return proto::FIELD_SEMANTIC_PRECIPITATION;
    } else {
        return proto::FIELD_SEMANTIC_DEFAULT;
    }
}

Property::Value_type
Property::Get_type_from_semantic(proto::Field_semantic sem)
{
    switch (sem) {
    case proto::FIELD_SEMANTIC_LATITUDE:
    case proto::FIELD_SEMANTIC_LONGITUDE:
        return Property::VALUE_TYPE_DOUBLE;
    case proto::FIELD_SEMANTIC_BOOL:
        return Property::VALUE_TYPE_BOOL;
    case proto::FIELD_SEMANTIC_ENUM:
    case proto::FIELD_SEMANTIC_ADSB_MODE:
    case proto::FIELD_SEMANTIC_AUTOPILOT_STATUS:
    case proto::FIELD_SEMANTIC_FLIGHT_MODE:
    case proto::FIELD_SEMANTIC_CONTROL_MODE:
    case proto::FIELD_SEMANTIC_GPS_FIX_TYPE:
        return Property::VALUE_TYPE_ENUM;
    case proto::FIELD_SEMANTIC_NUMERIC:
    case proto::FIELD_SEMANTIC_ACCEPTANCE_RADIUS:
    case proto::FIELD_SEMANTIC_ALTITUDE_AMSL:
    case proto::FIELD_SEMANTIC_ALTITUDE_AGL:
    case proto::FIELD_SEMANTIC_ALTITUDE_RAW:
    case proto::FIELD_SEMANTIC_HEADING:
    case proto::FIELD_SEMANTIC_VOLTAGE:
    case proto::FIELD_SEMANTIC_AIR_SPEED:
    case proto::FIELD_SEMANTIC_GROUND_SPEED:
    case proto::FIELD_SEMANTIC_VERTICAL_SPEED:
    case proto::FIELD_SEMANTIC_ROLL:
    case proto::FIELD_SEMANTIC_PITCH:
    case proto::FIELD_SEMANTIC_YAW:
    case proto::FIELD_SEMANTIC_RC_LINK_QUALITY:
    case proto::FIELD_SEMANTIC_GCS_LINK_QUALITY:
    case proto::FIELD_SEMANTIC_CURRENT:
    case proto::FIELD_SEMANTIC_FOV_H:
    case proto::FIELD_SEMANTIC_FOV_V:
    case proto::FIELD_SEMANTIC_GROUND_ELEVATION:
    case proto::FIELD_SEMANTIC_LOITER_RADIUS:
    case proto::FIELD_SEMANTIC_CAPACITY_LEVEL:
    case proto::FIELD_SEMANTIC_PRECIPITATION:
    case proto::FIELD_SEMANTIC_TEMPERATURE:
    case proto::FIELD_SEMANTIC_HUMIDITY:
        return Property::VALUE_TYPE_FLOAT;
    case proto::FIELD_SEMANTIC_SATELLITE_COUNT:
    case proto::FIELD_SEMANTIC_ICAO:
    case proto::FIELD_SEMANTIC_SQUAWK:
    case proto::FIELD_SEMANTIC_MILLISECONDS:
    case proto::FIELD_SEMANTIC_TIMESTAMP:
        return Property::VALUE_TYPE_INT;
    case proto::FIELD_SEMANTIC_STRING:
        return Property::VALUE_TYPE_STRING;
    case proto::FIELD_SEMANTIC_BINARY:
        return Property::VALUE_TYPE_BINARY;
    case proto::FIELD_SEMANTIC_LIST:
        return Property::VALUE_TYPE_LIST;
    case proto::FIELD_SEMANTIC_ANY:
        return Property::VALUE_TYPE_NONE;
    case proto::FIELD_SEMANTIC_DEFAULT:
        VSM_EXCEPTION(Invalid_param_exception, "No internal type for default semantic");
    }
    return Property::VALUE_TYPE_NONE;
}

#define ADD_BUILT_IN_ENUM(x) Add_enum(#x, proto::x)

Property::Property(
    int id,
    const std::string& name,
    proto::Field_semantic sem):
    semantic(sem), field_id(id), name(name),
    last_commit_time(std::chrono::steady_clock::now())
{
    if (sem == proto::FIELD_SEMANTIC_DEFAULT) {
        semantic = Get_default_semantic(name);
    }
    if (semantic == proto::FIELD_SEMANTIC_DEFAULT) {
        VSM_EXCEPTION(Invalid_param_exception, "No semantic specified for field %s", name.c_str());
    }

    type = Get_type_from_semantic(semantic);

    // Add built-in enum values.
    switch (semantic) {
    case proto::FIELD_SEMANTIC_ADSB_MODE:
        break;
    case proto::FIELD_SEMANTIC_CONTROL_MODE:
        ADD_BUILT_IN_ENUM(CONTROL_MODE_MANUAL);
        ADD_BUILT_IN_ENUM(CONTROL_MODE_AUTO);
        ADD_BUILT_IN_ENUM(CONTROL_MODE_CLICK_GO);
        ADD_BUILT_IN_ENUM(CONTROL_MODE_JOYSTICK);
        break;
    case proto::FIELD_SEMANTIC_GPS_FIX_TYPE:
        break;
    default:
        break;
    }
}

Property::Property(int id, const std::string& name, Value_type type):
    type(type), field_id(id), name(name),
    last_commit_time(std::chrono::steady_clock::now())
{
    switch (type) {
    case VALUE_TYPE_DOUBLE:
    case VALUE_TYPE_INT:
    case VALUE_TYPE_FLOAT:
        semantic = proto::FIELD_SEMANTIC_NUMERIC;
        break;
    case VALUE_TYPE_BOOL:
        semantic = proto::FIELD_SEMANTIC_BOOL;
        break;
    case VALUE_TYPE_STRING:
        semantic = proto::FIELD_SEMANTIC_STRING;
        break;
    case VALUE_TYPE_BINARY:
        semantic = proto::FIELD_SEMANTIC_BINARY;
        break;
    case VALUE_TYPE_ENUM:
        semantic = proto::FIELD_SEMANTIC_ENUM;
        break;
    case VALUE_TYPE_LIST:
        semantic = proto::FIELD_SEMANTIC_LIST;
        break;
    case VALUE_TYPE_NONE:
        semantic = proto::FIELD_SEMANTIC_ANY;
        break;
    }
}

Property::Property(Property::Ptr src)
{
    *this = *src;
}

bool
Property::Set_value(const proto::Field_value& v)
{
    is_changed = true;
    if (v.has_meta_value()) {
        switch (v.meta_value()) {
        case proto::META_VALUE_NA:
            value_spec = VALUE_SPEC_NA;
            return true;
        }
    }

    int64_t ival = 0;
    double dval = 0;

    if (v.has_double_value()) {
        ival = v.double_value();
        dval = v.double_value();
    } else if (v.has_float_value()) {
        ival = v.float_value();
        dval = v.float_value();
    } else if (v.has_int_value()) {
        ival = v.int_value();
        dval = v.int_value();
    }

    value_spec = VALUE_SPEC_REGULAR;
    switch (type) {
    case VALUE_TYPE_FLOAT:
    case VALUE_TYPE_DOUBLE:
        if (max_value && dval > max_value->double_value) {
            VSM_EXCEPTION(Invalid_param_exception,
                "Value %f exceeds specified max:%f", dval, max_value->double_value);
        }
        if (min_value && dval < min_value->double_value) {
            VSM_EXCEPTION(Invalid_param_exception,
                "Value %f is lower than specified min:%f", dval, min_value->double_value);
        }
        double_value = dval;
        return true;
    case VALUE_TYPE_ENUM:
        if (v.has_int_value()) {
            if (enum_values.find(ival) != enum_values.end()) {
                int_value = ival;
                return true;
            } else {
                VSM_EXCEPTION(Invalid_param_exception, "Value %" PRIi64 " is not part of enum %s", ival, name.c_str());
            }
        } else {
            VSM_EXCEPTION(Invalid_param_exception, "No int value found for enum");
        }
        break;
    case VALUE_TYPE_INT:
        if (max_value && ival > max_value->int_value) {
            VSM_EXCEPTION(Invalid_param_exception, "Value %" PRIi64 " exceeds specified max:%" PRIi64, ival, max_value->int_value);
        }
        if (min_value && ival < min_value->int_value) {
            VSM_EXCEPTION(Invalid_param_exception, "Value %" PRIi64 " lower than specified min:%" PRIi64, ival, max_value->int_value);
        }
        int_value = ival;
        return true;
    case VALUE_TYPE_STRING:
        if (v.has_string_value()) {
            string_value = v.string_value();
            return true;
        }
        break;
    case VALUE_TYPE_BINARY:
        if (v.has_bytes_value()) {
            string_value = v.bytes_value();
            return true;
        }
        break;
    case VALUE_TYPE_BOOL:
        if (v.has_bool_value()) {
            bool_value = v.bool_value();
            return true;
        }
        break;
    case VALUE_TYPE_LIST:
        if (v.has_list_value()) {
            list_value.CopyFrom(v.list_value());
            return true;
        }
        break;
    case VALUE_TYPE_NONE:
        // This determines the order of type detection from value: double, int, string, binary, bool, list.
        if (v.has_double_value()) {
            double_value = v.double_value();
            type = VALUE_TYPE_DOUBLE;
            return true;
        }
        if (v.has_float_value()) {
            double_value = v.float_value();
            type = VALUE_TYPE_FLOAT;
            return true;
        }
        if (v.has_int_value()) {
            int_value = v.int_value();
            type = VALUE_TYPE_INT;
            return true;
        }
        if (v.has_string_value()) {
            string_value = v.string_value();
            type = VALUE_TYPE_STRING;
            return true;
        }
        if (v.has_bytes_value()) {
            string_value = v.bytes_value();
            type = VALUE_TYPE_BINARY;
            return true;
        }
        if (v.has_bool_value()) {
            bool_value = v.bool_value();
            type = VALUE_TYPE_BOOL;
            return true;
        }
        if (v.has_list_value()) {
            list_value.CopyFrom(v.list_value());
            type = VALUE_TYPE_LIST;
            return true;
        }
        break;
    }
    return false;
}

void
Property::Set_timeout(int t)
{
    timeout = std::chrono::seconds(t);
}

void
Property::Add_enum(const std::string& name, int value)
{
    auto it = enum_values.emplace(value, name);
    if (it.second) {
        if (type != VALUE_TYPE_ENUM) {
            VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not enum", name.c_str(), type);
        }
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Invalid enum id %s", name.c_str());
    }
}

void
Property::Set_value(double v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_DOUBLE;
    }
    switch (type) {
    case VALUE_TYPE_DOUBLE:
    case VALUE_TYPE_FLOAT:
        // since NAN != NAN we need additional checks for isnan to avoid updating NAN with NAN.
        if ((double_value != v && (!std::isnan(v) || !std::isnan(double_value))) || value_spec != VALUE_SPEC_REGULAR) {
            double_value = v;
            is_changed = true;
        }
        break;
    case VALUE_TYPE_INT:
        if (int_value != v || value_spec != VALUE_SPEC_REGULAR) {
            int_value = v;
            is_changed = true;
        }
        break;
    case VALUE_TYPE_BOOL:
        if (bool_value != (v == 0) || value_spec != VALUE_SPEC_REGULAR) {
            bool_value = (v != 0);
            is_changed = true;
        }
        break;
    default:
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not double", name.c_str(), type);
    }
    value_spec = VALUE_SPEC_REGULAR;
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(int v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_INT;
    }
    switch (type) {
    case VALUE_TYPE_DOUBLE:
    case VALUE_TYPE_FLOAT:
        if (double_value != v || value_spec != VALUE_SPEC_REGULAR) {
            double_value = v;
            is_changed = true;
        }
        break;
    case VALUE_TYPE_INT:
    case VALUE_TYPE_ENUM:
        if (int_value != v || value_spec != VALUE_SPEC_REGULAR) {
            int_value = v;
            is_changed = true;
        }
        break;
    case VALUE_TYPE_BOOL:
        if (bool_value != (v != 0) || value_spec != VALUE_SPEC_REGULAR) {
            bool_value = v;
            is_changed = true;
        }
        break;
    default:
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not int", name.c_str(), type);
    }
    value_spec = VALUE_SPEC_REGULAR;
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(unsigned int v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_INT;
    }
    switch (type) {
    case VALUE_TYPE_DOUBLE:
    case VALUE_TYPE_FLOAT:
        if (double_value != v || value_spec != VALUE_SPEC_REGULAR) {
            double_value = v;
            is_changed = true;
        }
        break;
    case VALUE_TYPE_INT:
    case VALUE_TYPE_ENUM:
        if (int_value != static_cast<int>(v) || value_spec != VALUE_SPEC_REGULAR) {
            int_value = static_cast<int>(v);
            is_changed = true;
        }
        break;
    default:
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not int", name.c_str(), type);
    }
    value_spec = VALUE_SPEC_REGULAR;
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(bool v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_BOOL;
    }
    if (type == VALUE_TYPE_BOOL) {
        if (bool_value != v || value_spec != VALUE_SPEC_REGULAR) {
            bool_value = v;
            is_changed = true;
        }
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not bool", name.c_str(), type);
    }
    value_spec = VALUE_SPEC_REGULAR;
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(const char* v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_STRING;
    }
    if (v) {
        if (type == VALUE_TYPE_STRING || type == VALUE_TYPE_BINARY) {
            if (string_value != std::string(v) || value_spec != VALUE_SPEC_REGULAR) {
                string_value = std::string(v);
                is_changed = true;
                value_spec = VALUE_SPEC_REGULAR;
            }
        } else {
            VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not string", name.c_str(), type);
        }
    } else {
        Set_value_na();
    }
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(const std::string& v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_STRING;
    }
    if (type == VALUE_TYPE_STRING || type == VALUE_TYPE_BINARY) {
        if (string_value != v || value_spec != VALUE_SPEC_REGULAR) {
            string_value = v;
            is_changed = true;
            value_spec = VALUE_SPEC_REGULAR;
        }
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not string", name.c_str(), type);
    }
    update_time = std::chrono::system_clock::now();
}

void
Property::Set_value(const proto::List_value &v)
{
    if (type == VALUE_TYPE_NONE) {
        type = VALUE_TYPE_LIST;
    }
    if (type == VALUE_TYPE_LIST) {
        bool is_equal = false;
        if (v.values_size() != list_value.values_size()) {
            is_equal = false;
        } else {
            for (int i = 0; i < v.values_size(); i++) {
                if (!Fields_are_equal(v.values(i), list_value.values(i))) {
                    is_equal = false;
                    break;
                }
            }
        }
        if (!is_equal || value_spec != VALUE_SPEC_REGULAR) {
            list_value.CopyFrom(v);
            is_changed = true;
            value_spec = VALUE_SPEC_REGULAR;
        }
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Property %s type (%d) not list", name.c_str(), type);
    }
    update_time = std::chrono::system_clock::now();
}


void
Property::Set_value_na()
{
    if (value_spec != VALUE_SPEC_NA) {
        is_changed = true;
        value_spec = VALUE_SPEC_NA;
    }
    update_time = std::chrono::system_clock::now();
}

bool
Property::Get_value(bool &v)
{
    if (value_spec == VALUE_SPEC_REGULAR && type == VALUE_TYPE_BOOL) {
        v = bool_value;
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(float &v)
{
    if (value_spec == VALUE_SPEC_REGULAR) {
        switch (type) {
        case VALUE_TYPE_DOUBLE:
        case VALUE_TYPE_FLOAT:
            v = double_value;
            break;
        case VALUE_TYPE_INT:
        case VALUE_TYPE_ENUM:
            v = int_value;
            break;
        default:
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(double &v)
{
    if (value_spec == VALUE_SPEC_REGULAR) {
        switch (type) {
        case VALUE_TYPE_DOUBLE:
        case VALUE_TYPE_FLOAT:
            v = double_value;
            break;
        case VALUE_TYPE_INT:
        case VALUE_TYPE_ENUM:
            v = int_value;
            break;
        default:
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(std::string& v)
{
    if (value_spec == VALUE_SPEC_REGULAR && (type == VALUE_TYPE_STRING || type == VALUE_TYPE_BINARY)) {
        v = string_value;
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(int &v)
{
    int64_t val;
    if (Get_value(val)) {
        v = val;
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(int64_t &v)
{
    if (value_spec == VALUE_SPEC_REGULAR) {
        switch (type) {
        case VALUE_TYPE_DOUBLE:
        case VALUE_TYPE_FLOAT:
            v = double_value;
            break;
        case VALUE_TYPE_INT:
        case VALUE_TYPE_ENUM:
            v = int_value;
            break;
        default:
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool
Property::Get_value(proto::List_value &v)
{
    if (value_spec == VALUE_SPEC_REGULAR
            &&  type == VALUE_TYPE_LIST)
    {
                v.CopyFrom(list_value);
        return true;
    } else {
        return false;
    }
}

bool
Property::Is_value_na()
{
    return value_spec == VALUE_SPEC_NA;
}

void
Property::Set_changed()
{
    is_changed = true;
}

bool
Property::Is_changed()
{
    if (is_changed && type == VALUE_TYPE_STRING) {
        return true;
    }

    if (is_changed && std::chrono::steady_clock::now() - last_commit_time >= COMMIT_TIMEOUT) {
        return true;
    }
    if (!Is_value_na() && timeout.count()) {
        // timeout specified and value is still present.
        if ((std::chrono::system_clock::now() - update_time) > timeout) {
            // value expired, set to na.
            // LOG("Setting %d to na", field_id);
            Set_value_na();
            return true;
        }
    }
    return false;
};

Property::Ptr
Property::Default_value()
{
    if (default_value == nullptr) {
        default_value = Create(0, "", semantic);
    }
    return default_value;
}

Property::Ptr
Property::Min_value()
{
    if (min_value == nullptr) {
        min_value = Create(0, "", semantic);
    }
    return min_value;
}

Property::Ptr
Property::Max_value()
{
    if (max_value == nullptr) {
        max_value = Create(0, "", semantic);
    }
    return max_value;
}

void
Property::Register(proto::Register_field* field)
{
    field->Clear();
    field->set_name(name);
    field->set_field_id(field_id);
    field->set_semantic(semantic);
    for (auto& it : enum_values) {
        auto e = field->add_enumeration();
        e->set_id(it.first);
        e->set_description(it.second);
    }
    if (default_value) {
        default_value->Write_value(field->mutable_default_value());
    }
    if (min_value) {
        min_value->Write_value(field->mutable_min_value());
    }
    if (max_value) {
        max_value->Write_value(field->mutable_max_value());
    }
}

void
Property::Write_as_property(proto::Property_field* field)
{
    field->Clear();
    field->set_name(name);
    field->set_semantic(semantic);
    Write_value(field->mutable_value());
}

void
Property::Write_as_parameter(proto::Parameter_field* tf)
{
    tf->Clear();
    tf->set_field_id(field_id);
    Write_value(tf->mutable_value());
}

void
Property::Write_as_telemetry(proto::Telemetry_field* tf)
{
    tf->Clear();
    tf->set_field_id(field_id);
    Write_value(tf->mutable_value());
    is_changed = false;
    last_commit_time = std::chrono::steady_clock::now();
}

void
Property::Write_value(proto::Field_value* field)
{
    switch (value_spec) {
    case VALUE_SPEC_REGULAR:
        switch (type) {
        case VALUE_TYPE_DOUBLE:
            field->set_double_value(double_value);
            break;
        case VALUE_TYPE_INT:
        case VALUE_TYPE_ENUM:
            field->set_int_value(int_value);
            break;
        case VALUE_TYPE_FLOAT:
            field->set_float_value(double_value);
            break;
        case VALUE_TYPE_BOOL:
            field->set_bool_value(bool_value);
            break;
        case VALUE_TYPE_STRING:
            field->set_string_value(string_value);
            break;
        case VALUE_TYPE_BINARY:
            field->set_bytes_value(string_value);
            break;
        case VALUE_TYPE_LIST:
            field->set_allocated_list_value(&list_value);
            break;
        case VALUE_TYPE_NONE:
            VSM_EXCEPTION(Invalid_param_exception, "Property %s type not set", name.c_str());
            break;
        }
        break;
    case VALUE_SPEC_NA:
        field->set_meta_value(proto::META_VALUE_NA);
        break;
    }
}

std::string
Property::Dump_value()
{
    switch (value_spec) {
    case VALUE_SPEC_NA:
        return name + "(" + std::to_string(field_id) + ")= N/A";
    case VALUE_SPEC_REGULAR:
        switch (type) {
        case VALUE_TYPE_DOUBLE:
        case VALUE_TYPE_FLOAT:
            return name + "(" + std::to_string(field_id) + ")= " + std::to_string(double_value);
        case VALUE_TYPE_ENUM:
        case VALUE_TYPE_INT:
            return name + "(" + std::to_string(field_id) + ")= " + std::to_string(int_value);
        case VALUE_TYPE_BOOL:
            return name + "(" + std::to_string(field_id) + ")= " + std::to_string(bool_value);
        case VALUE_TYPE_STRING:
            return name + "(" + std::to_string(field_id) + ")= '" + string_value + "'";
        case VALUE_TYPE_BINARY:
            return name + "(" + std::to_string(field_id) + ") size=" + std::to_string(string_value.size());
        case VALUE_TYPE_NONE:
            return name + "(" + std::to_string(field_id) + ")= <none>";
        case VALUE_TYPE_LIST:
            return name + "(" + std::to_string(field_id) + ")= <size: " + std::to_string(list_value.values_size()) + ">";
        }
    }
    return name + "= <invalid>";
}

bool
Property::Fields_are_equal(const proto::Field_value& val1, const proto::Field_value& val2) {
    // do not check lists & meta
    if (val1.has_list_value() || val2.has_list_value() || val1.has_meta_value() || val2.has_meta_value()) {
        return false;
    }

    // double value
    if (val1.has_double_value()) {
        if (val2.has_double_value()) {
            if (val1.double_value() != val2.double_value()) {
                return false;
            }
        } else {
          return false;
        }
    }

    // bool
    if (val1.has_bool_value()) {
        if (val2.has_bool_value()) {
            if (val1.bool_value() != val2.bool_value()) {
                return false;
            }
        } else {
            return false;
        }
    }
    // float
    if (val1.has_float_value()) {
        if (val2.has_float_value()) {
            if (val1.float_value() != val2.float_value()) {
                return false;
            }
        } else {
            return false;
        }
    }

    // int
    if (val1.has_int_value()) {
        if (val2.has_int_value()) {
            if (val1.int_value() != val2.int_value()) {
                return false;
            }
        } else {
            return false;
        }
    }
    // string
    if (val1.has_string_value()) {
        if (val2.has_string_value()) {
            if (val1.string_value() != val2.string_value()) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool
Property::Is_equal(const Property& p)
{
    if (value_spec != p.value_spec || type != p.type) {
        return false;
    }
    switch (value_spec) {
    case VALUE_SPEC_NA:
        return true;
    case VALUE_SPEC_REGULAR:
        switch (type) {
        case VALUE_TYPE_DOUBLE:
        case VALUE_TYPE_FLOAT:
            return double_value == p.double_value;
        case VALUE_TYPE_ENUM:
        case VALUE_TYPE_INT:
            return int_value == p.int_value;
        case VALUE_TYPE_BOOL:
            return bool_value == p.bool_value;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_BINARY:
            return string_value == p.string_value;
        case VALUE_TYPE_NONE:
            return true;
        case VALUE_TYPE_LIST:
            if (list_value.values_size() == p.list_value.values_size()) {
                for (int i = 0; i < list_value.values_size(); i++) {
                    if (!Fields_are_equal(list_value.values(i), p.list_value.values(i))) {
                        return false;
                    }
                }
            } else {
                return false;
            }
            return true;
        }
    }
    return false;
}

bool
Property_list::Is_equal(const Property_list& plist)
{
    if (size() != plist.size()) {
        return false;
    }
    for (auto&f : plist) {
        auto it = find(f.first);
        if (it == plist.end()) {
            return false;
        } else {
            if (!f.second->Is_equal(*it->second)) {
                return false;
            }
        }
    }
    return true;
}
