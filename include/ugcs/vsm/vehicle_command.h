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
    };

    /** Construct command of a specific type. */
    Vehicle_command(Type type) : type(type)
    {
        ASSERT(type != Type::WAYPOINT);
    }

    /** Construct WAYPOINT. */
    Vehicle_command(const mavlink::ugcs::Pld_command_long_ex& cmd) : type(Type::WAYPOINT)
    {
        this->waypoint = Waypoint::Create(cmd);
    }

    /** Get type of the command. */
    Type
    Get_type() const
    {
        return type;
    }

    /** Data for a waypoint command. */
    class Waypoint : public std::enable_shared_from_this<Waypoint> {
        DEFINE_COMMON_CLASS(Waypoint, Waypoint)
    public:
        /** Construct waypoint command from the Mavlink. */
        Waypoint(const mavlink::ugcs::Pld_command_long_ex& cmd) :
            position(Geodetic_tuple(cmd->param5 * M_PI / 180.0,
                            cmd->param6 * M_PI / 180.0,
                            cmd->param7)),
            acceptance_radius(cmd->param1),
            speed(cmd->param3),
            takeoff_altitude(cmd->param2)
        {
        }

        /**
         * Target global position of the movement.
         */
        Wgs84_position position;

        /**
         * Acceptance radius of the target position in meters.
         * When the sphere with the radius centered at the target position is hit
         * by the vehicle the target position is considered reached.
         */
        double acceptance_radius;

        /**
         * Speed towards the waypoint.
         */
        double speed;

        /** Take-off point altitude from where the vehicle was launched. */
        double takeoff_altitude;
    };

    Waypoint
    Get_waypoint() const
    {
        ASSERT(type == Type::WAYPOINT && waypoint);
        return *waypoint;
    }

private:

    /** Type of the command. */
    const Type type;

    /** If type is WAYPOINT. */
    Waypoint::Ptr waypoint;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VEHICLE_COMMAND_H_ */
