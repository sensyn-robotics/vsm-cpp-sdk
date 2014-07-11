// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file telemetry_interface.h
 *
 * Interface for sending telemetry towards UCS server from user
 * vehicle instance.
 */
#ifndef _TELEMETRY_INTERFACE_H_
#define _TELEMETRY_INTERFACE_H_

#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/callback.h>

namespace ugcs {
namespace vsm {

/** Interface for sending internal raw telemetry messages towards UCS. */
class Telemetry_interface {
public:
    /** System status. */
    Callback_proxy<void, const mavlink::Pld_sys_status*>
        sys_status;

    /** Global position. */
    Callback_proxy<void, const mavlink::Pld_global_position_int*>
        global_position;

    /** Attitude. */
    Callback_proxy<void, const mavlink::Pld_attitude*>
        attitude;

    /** GPS raw. */
    Callback_proxy<void, const mavlink::Pld_gps_raw_int*>
        gps_raw;

    /** Raw IMU. */
    Callback_proxy<void, const mavlink::Pld_raw_imu*>
        raw_imu;

    /** Scaled pressure. */
    Callback_proxy<void, const mavlink::Pld_scaled_pressure*>
        scaled_pressure;

    /** VFR HUD. */
    Callback_proxy<void, const mavlink::Pld_vfr_hud*>
        vfr_hud;

    /** System status updated. */
    Callback_proxy<void> sys_status_update;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _TELEMETRY_INTERFACE_H_ */
