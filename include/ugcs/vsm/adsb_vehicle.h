// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

// Base class for aircraft reported by ADSB receiver
// Contains common telemetry fields based on received ADSB messages.
// All adsb receiver implementations should derive vehicle instances from this class.

#ifndef _UGCS_VSM_ADSB_VEHICLE_H_
#define _UGCS_VSM_ADSB_VEHICLE_H_

#include <ugcs/vsm/device.h>

class Adsb_vehicle: public ugcs::vsm::Device {
    DEFINE_COMMON_CLASS(Adsb_vehicle, Device)

public:
    Adsb_vehicle(uint32_t icao);

protected:
    ugcs::vsm::Property::Ptr t_latitude = nullptr;
    ugcs::vsm::Property::Ptr t_longitude = nullptr;
    // See proto::Adsb_altitude_source
    ugcs::vsm::Property::Ptr t_altitude_type = nullptr;
    ugcs::vsm::Property::Ptr t_altitude_amsl = nullptr;
    ugcs::vsm::Property::Ptr t_heading = nullptr;
    ugcs::vsm::Property::Ptr t_ground_speed = nullptr;
    ugcs::vsm::Property::Ptr t_vertical_speed = nullptr;
    ugcs::vsm::Property::Ptr t_callsign = nullptr;
    // See proto::Adsb_emitter_type
    ugcs::vsm::Property::Ptr t_emitter_type = nullptr;
    ugcs::vsm::Property::Ptr t_squawk = nullptr;

    ugcs::vsm::Subsystem::Ptr data_instance;
};

#endif /* _UGCS_VSM_ADSB_VEHICLE_H_ */
