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
        EMERGENCY_LAND
    };

    /** Construct command of a specific type. */
    Vehicle_command(Type type) : type(type)
    {

    }

    /** Get type of the command. */
    Type
    Get_type() const
    {
        return type;
    }

private:

    /** Type of the command. */
    const Type type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VEHICLE_COMMAND_H_ */
