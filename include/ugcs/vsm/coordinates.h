// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file coordinates.h
 *
 * Coordinates manipulation.
 */

#ifndef COORDINATES_H_
#define COORDINATES_H_

#include <ugcs/vsm/exception.h>

#include <cmath>

/* Might not be defined in <cmath> (not standard). */
#ifndef M_PI
/** PI constant. */
#define M_PI 3.14159265358979323846
#endif

namespace ugcs {
namespace vsm {

/** Datum for WGS84 geodetic system. */
class Wgs84_datum {
public:
    /** Flattening of the reference ellipsoid. */
    static constexpr double FLATTENING = 1.0 / 298.257223563;

    /** Equatorial radius of the reference ellipsoid, in meters. */
    static constexpr double EQUATORIAL_RADIUS = 6378137.0;

    /** Polar radius of the reference ellipsoid, in meters. */
    static constexpr double POLAR_RADIUS = 6356752.3;
};

/** Coordinates tuple for geodetic CS. */
class Geodetic_tuple {
public:
    /** Latitude value, radians. */
    double latitude,
    /** Longitude value, radians. */
           longitude,
    /** Altitude value, meters. */
           altitude;

    /** Construct the tuple with specific values. */
    Geodetic_tuple(double latitude, double longitude, double altitude):
        latitude(latitude), longitude(longitude), altitude(altitude)
    {}
};

/** Coordinates tuple for cartesian CS. */
class Cartesian_tuple {
public:
    // @{
    /** In meters. */
    double x, y, z;
    // @}

    /** Construct the tuple with specific values. */
    Cartesian_tuple(double x, double y, double z):
        x(x), y(y), z(z)
    {}
};

/** Immutable position in a specified coordinates system. */
template <class Datum>
class Position {
public:
    /** Associated datum. */
    typedef Datum Datum_type;

    /** Square of eccentricity of the reference ellipsoid. */
    static constexpr double ECCENTRICITY_SQUARED = (2 - Datum::FLATTENING) * Datum::FLATTENING;

    /** Construct position from geodetic coordinates. */
    Position(Geodetic_tuple tuple):
        coord(Normalize_latitude(tuple.latitude),
              Normalize_longitude(tuple.longitude),
              tuple.altitude),
        ecef_coord(Calculate_ecef(coord))
    {}

    /** Construct position from ECEF coordinates. */
    Position(Cartesian_tuple tuple):
        coord(From_ecef(tuple)), ecef_coord(tuple)
    {}

    /** Get representation in geodetic coordinates. */
    Geodetic_tuple
    Get_geodetic() const
    {
        return coord;
    }

    /** Get representation in ECEF coordinates - Earth-centered Earth-fixed CS,
     * ((x) axis points from the Earth center to the intersection of zero
     * parallel and zero (International Reference) meridian; (z) axis points
     * towards the International Reference Pole; (x, y, z) form right-hand
     * reference frame.
     */
    Cartesian_tuple
    Get_ecef() const
    {
        return ecef_coord;
    }

    /** One meter expressed in latitude radians. */
    double
    Lat_meter() const
    {
        //XXX valid only for WGS-84
        static constexpr double M1 = 111132.92;
        static constexpr double M2 = -559.82;
        static constexpr double M3 = 1.175;
        static constexpr double M4 = -0.0023;
        double latlen = M1 +
                       (M2 * cos(2.0 * coord.latitude)) +
                       (M3 * cos(4.0 * coord.latitude)) +
                       (M4 * cos(6.0 * coord.latitude));
        return M_PI / latlen / 180.0;
    }

     /** One meter expressed in longitude radians */
     double
     Long_meter() const
     {
         //XXX valid only for WGS-84
         static constexpr double P1 = 111412.84;
         static constexpr double P2 = -93.5;
         static constexpr double P3 = 0.118;
         double longlen = P1 * cos(coord.latitude) +
                          P2 * cos(3.0 * coord.latitude) +
                          P3 * cos(5.0 * coord.latitude);
        return M_PI / longlen / 180.0;
     }

     /** Get bearing in radians (-PI; +PI) to the target. */
     double
     Bearing(const Position &target)
     {
         double lat_m = (Lat_meter() + target.Lat_meter()) / 2.0;
         double long_m = (Long_meter() + target.Long_meter()) / 2.0;
         double d_lat = (target.coord.latitude - coord.latitude) / lat_m;
         double d_long = (target.coord.longitude - coord.longitude) / long_m;
         double d = sqrt(d_lat * d_lat + d_long * d_long);
         double r = d_lat / d;
         if (r > 1.0) {
             r = 1.0;
         } else if (r < -1.0) {
             r = -1.0;
         }
         double a = std::acos(r);
         if (d_long < 0) {
             a = -a;
         }
         return a;
     }

     /** The Earth's mean radius of curvature (averaging over all directions) at
      * a latitude of the position.
      */
     double
     Earth_radius() const
     {
         static constexpr double n =
                 Datum::EQUATORIAL_RADIUS * Datum::EQUATORIAL_RADIUS *
                 Datum::POLAR_RADIUS;
         double d1 = Datum::EQUATORIAL_RADIUS * std::cos(Get_geodetic().latitude);
         double d2 = Datum::POLAR_RADIUS * std::sin(Get_geodetic().latitude);
         return n / (d1 * d1 + d2 * d2);
     }

     /** Calculate the surface (altitude is not taken into account) distance in
      * meters between this and given positions using spherical law of cosines
      * formula and mean Earth radius of curvature between these points.
      */
     double
     Distance(const Position& pos) const
     {
         auto p1 = Get_geodetic();
         auto p2 = pos.Get_geodetic();
         Position avg(Geodetic_tuple((p1.latitude + p2.latitude) / 2, 0, 0));
         double acos_arg = std::sin(p1.latitude) * std::sin(p2.latitude) +
                 std::cos(p1.latitude) * std::cos(p2.latitude) *
                 std::cos(p2.longitude - p1.longitude);
         /* It was noticed (at least on Windows builds), that the formula above
          * might give values which are slightly more than 1 (or less than -1)
          * due to calculation errors. This happens when coordinates are very
          * close. Normalize it to be in the range [-1; 1] to avoid NaN result
          * from acos.
          */
         if (acos_arg > 1) {
             acos_arg = 1;
         } else if (acos_arg < -1) {
             acos_arg = -1;
         }
         return std::acos(acos_arg) * avg.Earth_radius();
     }

private:
    /** Internally stored in geodetic CS. */
    Geodetic_tuple coord;
    Cartesian_tuple ecef_coord;

    /** Normalize provided value so that it is in range (-base; base) wrapping
     * it as necessary.
     * @param value Value to normalize.
     * @param base Base value, should be positive.
     * @return Normalized value.
     */
    static double
    Normalize(double value, double base)
    {
        if (base <= 0) {
            VSM_EXCEPTION(Exception, "Invalid base specified");
        }
        return value - base * std::floor(value / base);
    }

    /** Normalize latitude value so that it is in range (-PI / 2; PI / 2). */
    static double
    Normalize_latitude(double value)
    {
        return Normalize(value + M_PI / 2, M_PI) - M_PI / 2;
    }

    /** Normalize longitude value so that it is in range (-PI; PI). */
    static double
    Normalize_longitude(double value)
    {
        return Normalize(value + M_PI, 2 * M_PI) - M_PI;
    }

    static double
    Signum(double value)
    {
        return (value == 0.0 || std::isnan(value)) ? value : std::copysign(1.0, value);
    }

    /** Convert ECEF coordinates to geodetic ones. */
    static Geodetic_tuple
    From_ecef(const Cartesian_tuple &tuple)
    {
        /* Threshold for iterative process set at 0.00001 arc second. */
        constexpr double threshold = 4.8481368110953605e-11;

        double latitude, longitude, altitude;
        double distance = std::hypot(tuple.x, tuple.y);

        if (distance == 0.0) {
            latitude = 0.5 * M_PI * Signum(tuple.z);
            longitude = 0.0;
            double sine_latitude = std::sin(latitude);
            altitude = tuple.z * sine_latitude - Datum::EQUATORIAL_RADIUS *
                std::sqrt(1 - ECCENTRICITY_SQUARED * sine_latitude * sine_latitude);
        } else {
            double raw_longitude = std::abs(std::asin(tuple.y / distance));
            if (tuple.y == 0.0) {
                longitude = tuple.x > 0.0 ? 0 : M_PI;
            } else {
                if (tuple.x > 0.0) {
                    longitude = tuple.y > 0.0 ? raw_longitude : 2.0 * M_PI - raw_longitude;
                } else {
                    longitude = tuple.y > 0.0 ? M_PI - raw_longitude : M_PI + raw_longitude;
                }
            }
            if (tuple.z == 0.0) {
                latitude = 0.0;
                altitude = distance - Datum::EQUATORIAL_RADIUS;
            } else {
                double radius = std::hypot(distance, tuple.z);
                double inclination = std::asin(tuple.z / radius);
                double ratio = ECCENTRICITY_SQUARED * Datum::EQUATORIAL_RADIUS / (2.0 * radius);

                /* Performing iterations. */
                double latitude_estimate, correction, next_correction = 0.0,
                        sine, root;
                int iterations = 0;
                constexpr static int max_iterations = 1000;
                do {
                    if (iterations > max_iterations) {
                        VSM_EXCEPTION(Exception, "Cannot find fixed point of the transform.");
                    }
                    iterations++;
                    correction = next_correction;
                    latitude_estimate = inclination + correction;
                    sine = std::sin(latitude_estimate);
                    root = std::sqrt(1.0 - ECCENTRICITY_SQUARED * sine * sine);
                    next_correction =
                        std::asin(ratio * std::sin(2.0 * latitude_estimate) / root);
                } while (std::abs(next_correction - correction) >= threshold);

                latitude = latitude_estimate;
                altitude = distance * std::cos(latitude) + tuple.z * sine -
                    Datum::EQUATORIAL_RADIUS * root;
            }
        }
        return Geodetic_tuple(latitude, longitude, altitude);
    }

    static Cartesian_tuple
    Calculate_ecef(const Geodetic_tuple &coord)
    {
        double cosine_latitude = std::cos(coord.latitude);
        double sine_latitude = std::sin(coord.latitude);
        double cosine_longitude = std::cos(coord.longitude);
        double sine_longitude = std::sin(coord.longitude);

        double curvature_radius = Datum::EQUATORIAL_RADIUS /
            std::sqrt(1.0 - ECCENTRICITY_SQUARED * sine_latitude * sine_latitude);

        return Cartesian_tuple(
            (curvature_radius + coord.altitude) * cosine_latitude * cosine_longitude,
            (curvature_radius + coord.altitude) * cosine_latitude * sine_longitude,
            ((1.0 - ECCENTRICITY_SQUARED) * curvature_radius + coord.altitude) * sine_latitude
        );
    }
};

/** Position defined in WGS84 geodetic system. */
typedef Position<Wgs84_datum> Wgs84_position;

float
Normalize_angle_0_2pi(float a);

float
Normalize_angle_minuspi_pi(float a);

} /* namespace vsm */
} /* namespace ugcs */

#endif /* COORDINATES_H_ */
