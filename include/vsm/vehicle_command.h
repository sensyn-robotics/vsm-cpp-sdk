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

#include <vsm/mavlink.h>

namespace vsm {

/** Information about a command for a vehicle. */
class Vehicle_command {
public:
    /** Type of the command. */
    enum class Type {
        /** Start/resume current mission. */
        GO,
        /** Pause current mission and hold at current position. */
        HOLD
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

#endif /* _VEHICLE_COMMAND_H_ */
