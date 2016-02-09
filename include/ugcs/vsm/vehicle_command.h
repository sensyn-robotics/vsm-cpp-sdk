// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle_command.h
 *
 * VSM vehicle command payload definition.
 */
#ifndef _VEHICLE_COMMAND_H_
#define _VEHICLE_COMMAND_H_

#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/coordinates.h>

namespace ugcs {
namespace vsm {

/** Information about a command for a vehicle. */
class Vehicle_command {
public:
    /** Type of the command. */
    enum class Type {
        /** Do arm. */
        ARM,
        /** Do disarm. */
        DISARM,
        /** Enable auto mode. */
        AUTO_MODE,
        /** Enable manual mode. */
        MANUAL_MODE,
        /** Enable guided mode. */
        GUIDED_MODE,
        /** Return to home. */
        RETURN_HOME,
        /** Do takeoff. */
        TAKEOFF,
        /** Do land. */
        LAND,
        /** Do emergency land. */
        EMERGENCY_LAND,
        /** Trigger the camera, take snapshot. */
        CAMERA_TRIGGER,
        /** Fly to this waypoint. */
        WAYPOINT,
        /** Pause the mission. */
        PAUSE_MISSION,
        /** Resume the mission. */
        RESUME_MISSION,
        /** Set transponder ICAO and registration ID */
        ADSB_INSTALL,
        /** Set transponder flight ID */
        ADSB_PREFLIGHT,
        /** Set transponder mode */
        ADSB_OPERATING,
    };


    /** Construct command of a specific type. */
    Vehicle_command(Type type, const mavlink::ugcs::Pld_command_long_ex& cmd);

    /** Construct install message. */
    Vehicle_command(const mavlink::ugcs::Pld_adsb_transponder_install&);

    /** Construct preflight message. */
    Vehicle_command(const mavlink::ugcs::Pld_adsb_transponder_preflight& );

    /** Get type of the command. */
    Type
    Get_type() const
    {
        return type;
    }

    // meters
    float
    Get_acceptance_radius() const
    {
        return acceptance_radius;
    }

    // m/s
    float
    Get_speed() const
    {
        return speed;
    }

    // clockwise radians from North
    float
    Get_heading() const
    {
        return heading;
    }

    // Takeoff altitude AMSL.
    float
    Get_takeoff_altitude() const
    {
        return takeoff_altitude;
    }

    // WP latitude.
    float
    Get_latitude() const
    {
        return position.Get_geodetic().latitude;
    }

    // WP longitude.
    float
    Get_longitude() const
    {
        return position.Get_geodetic().longitude;
    }

    // WP altitude AMSL.
    float
    Get_altitude() const
    {
        return position.Get_geodetic().altitude;
    }

    // ADSB flight number
    std::string
    Get_adsb_flight_id() const
    {
        return string1;
    }

    // ADSB registration number
    std::string
    Get_adsb_registration() const
    {
        return string1;
    }

    uint32_t
    Get_adsb_icao_code() const
    {
        return integer1;
    }

    mavlink::Int32
    Get_adsb_operating_mode() const
    {
        return integer1;
    }

    mavlink::Int32
    Get_adsb_ident_on() const
    {
        return integer2;
    }

    mavlink::Int32
    Get_adsb_squawk() const
    {
        return integer3;
    }

private:

    /** Type of the command. */
    const Type type;

    Wgs84_position position;

    /**
     * Acceptance radius of the target position in meters.
     * When the sphere with the radius centered at the target position is hit
     * by the vehicle the target position is considered reached.
     */
    float acceptance_radius;

    /**
     * Speed towards the waypoint.
     */
    float speed;

    /** Heading. radians from North*/
    float heading;

    /** Take-off point altitude AMSL from where the vehicle was launched. */
    float takeoff_altitude;

    std::string string1;

    mavlink::Int32 integer1;
    mavlink::Int32 integer2;
    mavlink::Int32 integer3;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VEHICLE_COMMAND_H_ */
