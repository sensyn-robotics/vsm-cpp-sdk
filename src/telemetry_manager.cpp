// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/telemetry_manager.h>

#include <cmath>

using namespace ugcs::vsm;

/* ****************************************************************************/
/* Telemetry_manager::Report class. */

Telemetry_manager::Report::Report(Telemetry_manager &manager,
                                  std::chrono::milliseconds timestamp):
    manager(manager), timestamp(timestamp)
{}

Telemetry_manager::Report::~Report()
{
    Commit();
}

void
Telemetry_manager::Report::Commit()
{
    std::unique_lock<std::mutex> lock(manager.mutex);
    Raw_data prev_data(manager.last_data);
    for (std::unique_ptr<tm::internal::Param> &param: params) {
        param->Commit(manager);
    }
    params.clear();
    manager.Commit(timestamp, prev_data);
}

/* ****************************************************************************/
/* Telemetry_manager::Raw_data class. */

void
Telemetry_manager::Raw_data::Reset()
{
    memset(this, 0, sizeof(*this));
    sys_status.Reset();
    position.Reset();
    attitude.Reset();
    gps_raw.Reset();
    raw_imu.Reset();
    scaled_pressure.Reset();
    vfr_hud.Reset();
}

void
Telemetry_manager::Raw_data::Hit_payload(Payload_type type)
{
    ASSERT(type < Payload_type::NUM_PAYLOAD_TYPES);
    payload_generation[type]++;
}

bool
Telemetry_manager::Raw_data::Is_dirty(Payload_type type, const Raw_data &prev_data)
{
    ASSERT(type < Payload_type::NUM_PAYLOAD_TYPES);
    if (payload_generation[type] != prev_data.payload_generation[type]) {
        return true;
    }
    switch (type) {
    case Payload_type::SYS_STATUS:
        return sys_status != prev_data.sys_status;
    case Payload_type::POSITION:
        return position != prev_data.position;
    case Payload_type::ATTITUDE:
        return attitude != prev_data.attitude;
    case Payload_type::GPS_RAW:
        return gps_raw != prev_data.gps_raw;
    case Payload_type::RAW_IMU:
        return raw_imu != prev_data.raw_imu;
    case Payload_type::SCALED_PRESSURE:
        return scaled_pressure != prev_data.scaled_pressure;
    case Payload_type::VFR_HUD:
        return vfr_hud != prev_data.vfr_hud;

    /* Prevent from compilation warning and still allow compiler to control that
     * all values are handled.
     */
    case Payload_type::NUM_PAYLOAD_TYPES:
        ASSERT(false);
    }
    return false;
}

void
Telemetry_manager::Raw_data::Commit(const Telemetry_interface &iface,
                                    std::chrono::milliseconds timestamp,
                                    const Raw_data &prev_data)
{
    if (Is_dirty(Payload_type::SYS_STATUS, prev_data)) {
        if (iface.sys_status) {
            iface.sys_status(&sys_status);
        }
    }

    if (Is_dirty(Payload_type::POSITION, prev_data)) {
        position->time_boot_ms = timestamp.count();
        if (iface.global_position) {
            iface.global_position(&position);
        }
    }

    if (Is_dirty(Payload_type::ATTITUDE, prev_data)) {
        attitude->time_boot_ms = timestamp.count();
        if (iface.attitude) {
            iface.attitude(&attitude);
        }
    }

    if (Is_dirty(Payload_type::GPS_RAW, prev_data)) {
        gps_raw->time_usec = std::chrono::microseconds(timestamp).count();
        if (iface.gps_raw) {
            iface.gps_raw(&gps_raw);
        }
    }

    if (Is_dirty(Payload_type::RAW_IMU, prev_data)) {
        raw_imu->time_usec = std::chrono::microseconds(timestamp).count();
        if (iface.raw_imu) {
            iface.raw_imu(&raw_imu);
        }
    }

    if (Is_dirty(Payload_type::SCALED_PRESSURE, prev_data)) {
        scaled_pressure->time_boot_ms = timestamp.count();
        if (iface.scaled_pressure) {
            iface.scaled_pressure(&scaled_pressure);
        }
    }

    if (Is_dirty(Payload_type::VFR_HUD, prev_data)) {
        if (iface.vfr_hud) {
            iface.vfr_hud(&vfr_hud);
        }
    }
}

/* ****************************************************************************/
/* Telemetry_manager class. */

void
Telemetry_manager::Register_transport_interface(Telemetry_interface &iface)
{
    std::unique_lock<std::mutex> lock(mutex);
    this->iface = iface;
}

void
Telemetry_manager::Unregister_transport_interface()
{
    std::unique_lock<std::mutex> lock(mutex);
    this->iface = Telemetry_interface();
}

Telemetry_manager::Report::Ptr
Telemetry_manager::Open_report(std::chrono::milliseconds timestamp)
{
    return std::make_shared<Report>(*this, timestamp);
}

void
Telemetry_manager::Sys_status_update()
{
    if (iface.sys_status_update) {
        iface.sys_status_update();
    }
}

void
Telemetry_manager::Commit(std::chrono::milliseconds timestamp,
                          const Raw_data &prev_data)
{
    last_data.Commit(iface, timestamp, prev_data);
}

/* ****************************************************************************/
/* Committers. */

void
Telemetry_manager::Commit(const tm::Battery_voltage::Base &value)
{
    last_data.Hit_payload(Raw_data::Payload_type::SYS_STATUS);
    if (!value) {
        last_data.sys_status->voltage_battery.Reset();
        return;
    }

    if (value.value * 1000 > 0xfffe) {
        last_data.sys_status->voltage_battery = 0xfffe;
    } else {
        last_data.sys_status->voltage_battery = value.value * 1000;
    }
}

void
Telemetry_manager::Commit(const tm::Battery_current::Base &value)
{
    last_data.Hit_payload(Raw_data::Payload_type::SYS_STATUS);
    if (!value) {
        last_data.sys_status->current_battery.Reset();
        return;
    }

    if (value.value * 100 > 0xfffe) {
        last_data.sys_status->current_battery = 0xfffe;
    } else {
        last_data.sys_status->current_battery = value.value * 100;
    }
}

void
Telemetry_manager::Commit(const tm::Position::Base &value)
{
    if (!value) {
        return;
    }

    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    last_data.Hit_payload(Raw_data::Payload_type::GPS_RAW);
    last_data.Hit_payload(Raw_data::Payload_type::VFR_HUD);

    Geodetic_tuple gt = value.value.Get_geodetic();

    last_data.position->lat = gt.latitude / M_PI * 180.0 * 1e7;
    last_data.position->lon = gt.longitude / M_PI * 180.0 * 1e7;
    last_data.position->alt = gt.altitude * 1000.0;

    last_data.gps_raw->lat = last_data.position->lat;
    last_data.gps_raw->lon = last_data.position->lon;
    last_data.gps_raw->alt = last_data.position->alt;

    last_data.vfr_hud->alt = gt.altitude;
}

void
Telemetry_manager::Commit(const tm::Abs_altitude::Base &value)
{
    if (!value) {
        return;
    }

    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    last_data.Hit_payload(Raw_data::Payload_type::GPS_RAW);
    last_data.Hit_payload(Raw_data::Payload_type::VFR_HUD);

    last_data.position->alt = value.value * 1000.0;
    last_data.gps_raw->alt = value.value * 1000.0;
    last_data.vfr_hud->alt = value.value;
}

void
Telemetry_manager::Commit(const tm::Rel_altitude::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    last_data.position->relative_alt = value.value * 1000.0;
}

void
Telemetry_manager::Commit(const tm::Gps_satellites_count::Base &value)
{
    last_data.Hit_payload(Raw_data::Payload_type::GPS_RAW);
    if (!value) {
        last_data.gps_raw->satellites_visible.Reset();
        return;
    }
    if (value.value > 254) {
        last_data.gps_raw->satellites_visible = 254;
    } else {
        last_data.gps_raw->satellites_visible = value.value;
    }
}

void
Telemetry_manager::Commit(const tm::Gps_fix_type::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::GPS_RAW);
    switch (value.value) {
    case tm::Gps_fix_type_e::NONE:
        last_data.gps_raw->fix_type = 0;
        break;
    case tm::Gps_fix_type_e::_2D:
        last_data.gps_raw->fix_type = 2;
        break;
    case tm::Gps_fix_type_e::_3D:
        last_data.gps_raw->fix_type = 3;
        break;
    }
}

void
Telemetry_manager::Commit(const tm::Course::Base &value)
{
    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    last_data.Hit_payload(Raw_data::Payload_type::VFR_HUD);
    if (!value) {
        last_data.position->hdg.Reset();
        last_data.vfr_hud->heading.Reset();
        return;
    }
    // normalize the angle value into 0..2Pi
    auto normalized = fmod(value.value, 2 * M_PI);
    if (normalized < 0) {
        normalized += 2 * M_PI;
    }
    last_data.position->hdg = (normalized / M_PI) * 180.0 * 100.0;
    last_data.vfr_hud->heading = std::lround((normalized / M_PI) * 180.0);
}

void
Telemetry_manager::Commit(const tm::Attitude::Pitch::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::ATTITUDE);
    // normalize the angle value into -Pi..+Pi
    auto normalized = fmod(value.value, 2 * M_PI);
    if (normalized < -M_PI) {
        normalized += 2 * M_PI;
    } else if (normalized > M_PI) {
        normalized -= 2 * M_PI;
    }
    last_data.attitude->pitch = normalized;
}

void
Telemetry_manager::Commit(const tm::Attitude::Roll::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::ATTITUDE);
    // normalize the angle value into -Pi..+Pi
    auto normalized = fmod(value.value, 2 * M_PI);
    if (normalized < -M_PI) {
        normalized += 2 * M_PI;
    } else if (normalized > M_PI) {
        normalized -= 2 * M_PI;
    }
    last_data.attitude->roll = normalized;
}

void
Telemetry_manager::Commit(const tm::Attitude::Yaw::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::ATTITUDE);
    // normalize the angle value into -Pi..+Pi
    auto normalized = fmod(value.value, 2 * M_PI);
    if (normalized < -M_PI) {
        normalized += 2 * M_PI;
    } else if (normalized > M_PI) {
        normalized -= 2 * M_PI;
    }
    last_data.attitude->yaw = normalized;
}

void
Telemetry_manager::Commit(const tm::Ground_speed::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    double vx, vy;
    if (last_data.position->hdg.Is_reset()) {
        vx = value.value;
        vy = 0;
    } else {
        /* Calculate projections. */
        double hdg = static_cast<double>(last_data.position->hdg) / 18000.0 * M_PI;
        vx = value.value * cos(hdg);
        vy = value.value * sin(hdg);
    }

    if (vx * 100 > SHRT_MAX) {
        last_data.position->vx = SHRT_MAX;
    } else if (vx * 100 < SHRT_MIN) {
        last_data.position->vx = SHRT_MIN;
    } else {
        last_data.position->vx = vx * 100;
    }

    if (vy * 100 > SHRT_MAX) {
        last_data.position->vy = SHRT_MAX;
    } else if (vy * 100 < SHRT_MIN) {
        last_data.position->vy = SHRT_MIN;
    } else {
        last_data.position->vy = vy * 100;
    }
}

void
Telemetry_manager::Commit(const tm::Climb_rate::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::POSITION);
    last_data.Hit_payload(Raw_data::Payload_type::VFR_HUD);
    last_data.vfr_hud->climb = value.value;
    if (value.value * 100 > SHRT_MAX) {
        last_data.position->vz = SHRT_MAX;
    } else if (value.value * 100 < SHRT_MIN) {
        last_data.position->vz = SHRT_MIN;
    } else {
        last_data.position->vz = value.value * 100;
    }
}

void
Telemetry_manager::Commit(const tm::Link_quality::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::SYS_STATUS);
    double rate = value.value;
    if (rate > 1.0) {
        rate = 1.0;
    } else if (rate < 0.0) {
        rate = 0.0;
    }
    rate = 1.0 - rate;
    last_data.sys_status->drop_rate_comm = 10000 * rate;
}

void
Telemetry_manager::Commit(const tm::Rclink_quality::Base &value)
{
    if (!value) {
        return;
    }
    last_data.Hit_payload(Raw_data::Payload_type::SYS_STATUS);
    double rate = value.value;
    if (rate > 1.0) {
        rate = 1.0;
    } else if (rate < 0.0) {
        rate = 0.0;
    }
    rate = 1.0 - rate;
    last_data.sys_status->errors_comm = 10000 * rate;
}
