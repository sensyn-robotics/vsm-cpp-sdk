// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle_command.h
 *
 * VSM vehicle command payload definition.
 */
#ifndef _UGCS_VSM_VEHICLE_COMMAND_H_
#define _UGCS_VSM_VEHICLE_COMMAND_H_

#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/property.h>


namespace ugcs {
namespace vsm {

/** Information about a command for a vehicle. */
class Vehicle_command {
public:
    /** Camera trigger state. */
    enum class Camera_trigger_state {
        SINGLE_SHOT,
        VIDEO_START,
        VIDEO_STOP,
        // Toggle video recording on/off.
        // Used for cameras which do not support separate commands for video start/stop.
        VIDEO_TOGGLE,
        UNKNOWN
    };

    /** Camera power state. */
    enum class Camera_power_state {
        ON,
        OFF,
        // Toggle the power state.
        // Used for cameras which do not support separate commands for on and off.
        TOGGLE,
        UNKNOWN
    };

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
        /** Enable direct vehicle control mode. */
        JOYSTICK_CONTROL_MODE,
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
        /** Direct vehicle control. */
        DIRECT_VEHICLE_CONTROL,
        /** Direct gimbal control. */
        DIRECT_PAYLOAD_CONTROL,
        /** camera power on/off. */
        CAMERA_POWER,
        /** camera selection. */
        CAMERA_VIDEO_SOURCE,
    };

    // Construct from argument list.
    Vehicle_command(Type type, const Property_list& params);

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

    float
    Get_pitch() const
    {
        return pitch;
    }

    float
    Get_roll() const
    {
        return roll;
    }

    float
    Get_yaw() const
    {
        return yaw;
    }

    float
    Get_throttle() const
    {
        return throttle;
    }

    float
    Get_zoom() const
    {
        return zoom;
    }

    int
    Get_payload_id() const
    {
        return payload_id;
    }

private:
    /** Type of the command. */
    Type type;

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

    /** Vertical speed m/s */
    float vertical_speed = 0;

    /** Roll [-1..1] Used in direct control messages. */
    float roll;
    /** Pitch [-1..1] Used in direct control messages. */
    float pitch;
    /** Yaw [-1..1] Used in direct control messages. */
    float yaw;
    /** Throttle [-1..1] Used in direct control messages. */
    float throttle;
    /** Zoom [-1..1] Used in direct control messages. */
    float zoom;

    /** Take-off point altitude AMSL from where the vehicle was launched. */
    float takeoff_altitude;

    /** Payload ID. currently:  1 - Primary, 2 - Secondary */
    int payload_id; // DEPRECATED

    Camera_power_state power_state;
    Camera_trigger_state trigger_state;

    std::string string1;

    mavlink::Int32 integer1;
    mavlink::Int32 integer2;
    mavlink::Int32 integer3;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_VEHICLE_COMMAND_H_ */
