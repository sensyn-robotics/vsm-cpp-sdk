// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
#include <ugcs/vsm/vehicle_command.h>

using namespace ugcs::vsm;

/** Construct command with parameters. */
Vehicle_command::Vehicle_command(Type type, const mavlink::ugcs::Pld_command_long_ex& cmd):
    type(type),
    position(Geodetic_tuple(cmd->param5 * M_PI / 180.0,
                    cmd->param6 * M_PI / 180.0,
                    cmd->param7))
{
    switch (type) {
    case Type::WAYPOINT:
        acceptance_radius = cmd->param1.Get();
        takeoff_altitude = cmd->param2.Get();
        speed = cmd->param3.Get();
        heading = cmd->param4 * M_PI / 180.0;
        break;
    case Type::ADSB_OPERATING:
        if (cmd->param1.Is_reset()) {
            integer1.Reset();
        } else {
            integer1 = cmd->param1.Get();
        }
        if (cmd->param2.Is_reset()) {
            integer2.Reset();
        } else {
            integer2 = cmd->param2.Get();
        }
        if (cmd->param3.Is_reset()) {
            integer3.Reset();
        } else {
            integer3 = cmd->param3.Get();
        }
        break;
    default:
        // Other commands do not have parameters.
        break;
    }
}

/** Construct install message. */
Vehicle_command::Vehicle_command(const mavlink::ugcs::Pld_adsb_transponder_install& cmd):
    type(Type::ADSB_INSTALL),
    position(Geodetic_tuple(0, 0, 0))
{
    integer1 = cmd->icao_code;
    string1 = cmd->registration;
}

/** Construct preflight message. */
Vehicle_command::Vehicle_command(const mavlink::ugcs::Pld_adsb_transponder_preflight& cmd):
    type(Type::ADSB_PREFLIGHT),
    position(Geodetic_tuple(0, 0, 0))
{
    string1 = cmd->flight;
}
