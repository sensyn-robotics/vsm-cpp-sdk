// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/adsb_vehicle.h>

using namespace ugcs::vsm;

Adsb_vehicle::Adsb_vehicle(uint32_t icao_code):
    Device(proto::DEVICE_TYPE_ADSB_VEHICLE)
{
    data_instance = Add_subsystem(proto::SUBSYSTEM_TYPE_ADSB_VEHICLE);
    // Create telemetry
#define ADD_T(x) t_##x = data_instance->Add_telemetry(#x);
#define ADD_TS(x, y) t_##x = data_instance->Add_telemetry(#x, y);

    ADD_T(latitude);
    ADD_T(longitude);
    ADD_TS(altitude_type, proto::FIELD_SEMANTIC_ENUM);
    ADD_T(altitude_amsl);
    ADD_T(heading);
    ADD_T(ground_speed);
    ADD_T(vertical_speed);
    ADD_TS(callsign, proto::FIELD_SEMANTIC_STRING);
    ADD_TS(emitter_type, proto::FIELD_SEMANTIC_ENUM);
    ADD_TS(squawk, proto::FIELD_SEMANTIC_SQUAWK);
    Set_property("icao", icao_code);
}
