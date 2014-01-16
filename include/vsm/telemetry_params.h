// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file telemetry_params.h
 *
 * All telemetry parameters accepted from VSM.
 */
#ifndef TELEMETRY_PARAMS_H_
#define TELEMETRY_PARAMS_H_

#include <vsm/coordinates.h>

namespace vsm {

class Telemetry_manager;

/** All telemetry parameters are defined in this namespace. */
namespace telemetry {

namespace internal {

/** Base interface for all parameters. */
class Param {
public:
    /** Commit dispatcher. */
    virtual void
    Commit(Telemetry_manager &tm_man) const = 0;
};

/** Helper base implementation for all parameters. */
template <class ParamType, typename Value_type,
          class Telemetry_manager_type = Telemetry_manager>
class Param_base: public Param {
public:
    /** Stored parameter value. */
    Value_type value;
    /** Is the value initialized. */
    bool is_valid = true;

    /** Construct with uninitialized parameter value. */
    Param_base():
        is_valid(false)
    {}

    /** Construct and assign the provided value.
     *
     * @param value Initial value for the parameter.
     */
    Param_base(const Value_type &value):
        value(value)
    {}

    /** Commit current value to the telemetry manager.
     *
     * @param tm_man Telemetry manager object to commit to.
     */
    virtual void
    Commit(Telemetry_manager_type &tm_man) const override
    {
        tm_man.Commit(*this);
    }

    /** Check if the value is valid. */
    explicit operator bool() const
    {
        return is_valid;
    }
};

} /* namespace internal */

/* All telemetry parameters follow. */

/** Battery voltage, V. */
class Battery_voltage: public internal::Param_base<Battery_voltage, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Battery current, A. */
class Battery_current: public internal::Param_base<Battery_current, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** GPS coordinates. */
class Position: public internal::Param_base<Position, Wgs84_position> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;

    Position():
        Position(Geodetic_tuple(0, 0, 0))
    {
        is_valid = false;
    }

    /** Construct position parameter using geodetic tuple.
     *
     * @param pos Position as geodetic tuple with WGS-84 coordinates.
     */
    Position(const Geodetic_tuple &pos):
        Position(Wgs84_position(pos))
    {}

    /** Construct position parameter using cartesian tuple.
     *
     * @param pos Position as cartesian tuple with ECEF coordinates.
     */
    Position(const Cartesian_tuple &pos):
        Position(Wgs84_position(pos))
    {}
};

/** Absolute altitude (MSL), m. */
class Abs_altitude: public internal::Param_base<Abs_altitude, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Relative altitude, m. Usually relatively to arming/launch point. */
class Rel_altitude: public internal::Param_base<Rel_altitude, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Number of GPS satellites visible. */
class Gps_satellites_count: public internal::Param_base<Gps_satellites_count, unsigned> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Values for GPS fix type. */
enum class Gps_fix_type_e {
    /** No fix. */
    NONE,
    /** 2D fix. */
    _2D,
    /** 3D fix. */
    _3D
};

/** Number of GPS satellites visible. */
class Gps_fix_type: public internal::Param_base<Gps_fix_type, Gps_fix_type_e> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Heading, deg. */
class Heading: public internal::Param_base<Heading, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Attitude pitch angle, rad [-pi;+pi]. */
class Pitch: public internal::Param_base<Pitch, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Attitude roll angle, rad [-pi;+pi]. */
class Roll: public internal::Param_base<Roll, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Attitude yaw angle, rad [-pi;+pi]. */
class Yaw: public internal::Param_base<Yaw, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Ground speed module, m/s. */
class Ground_speed: public internal::Param_base<Ground_speed, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Climb rate, m/s. */
class Climb_rate: public internal::Param_base<Climb_rate, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

/** Link quality is inversely proportional to packet drop rate on a link,
 * 1.0 is the best quality, 0.0 is total link failure.
 */
class Link_quality: public internal::Param_base<Link_quality, double> {
public:
    using Param_base::Param_base;
    /** Base class type. */
    typedef Param_base Base;
};

} /* namespace telemetry */

/** Alias for telemetry namespace. */
namespace tm = telemetry;

} /* namespace vsm */

#endif /* TELEMETRY_PARAMS_H_ */
