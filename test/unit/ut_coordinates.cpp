// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Wgs84_datum class.
 */

#include <ugcs/vsm/coordinates.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

TEST(basic_functionality)
{
    Wgs84_position(Geodetic_tuple(0, 0, 0));
    Wgs84_position(Cartesian_tuple(0, 0, 0));
}

TEST(distance_zero)
{
    Geodetic_tuple t1(0.42, 0.24, 0);
    Geodetic_tuple t2(0.42, 0.24, 0);
    CHECK_CLOSE(0, Wgs84_position(t1).Distance(t2), 0.5);
}

TEST(distance_somewhere_in_Georgia)
{
    Geodetic_tuple t1(42. / 180 * M_PI, 42.1 / 180 * M_PI, 0);
    Geodetic_tuple t2(42.1 / 180 * M_PI, 42. / 180 * M_PI, 0);
    CHECK_CLOSE(13860.5, Wgs84_position(t1).Distance(t2), 0.5);
}

TEST(bearing)
{
    Geodetic_tuple t1(42. / 180 * M_PI, 42. / 180 * M_PI, 0);
    Geodetic_tuple t2(42.1 / 180 * M_PI, 42.1 / 180 * M_PI, 0);
    CHECK_CLOSE(36.7, Wgs84_position(t1).Bearing(t2) * 180 / M_PI, 0.1);
}
