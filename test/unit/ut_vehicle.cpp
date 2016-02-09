// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Test for vehicle class.
 */

#include <ugcs/vsm/vehicle.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

class Some_vehicle: public Vehicle
{
    DEFINE_COMMON_CLASS(Some_vehicle, Vehicle)
public:
    int some_prop;

    Some_vehicle(int some_prop):
        Vehicle(mavlink::MAV_TYPE::MAV_TYPE_QUADROTOR,
                mavlink::MAV_AUTOPILOT::MAV_AUTOPILOT_ARDUPILOTMEGA,
                Vehicle::Capabilities(),
                "123456", "SuperCopter"),
                some_prop(some_prop)
    {}
};

TEST(basic_usage)
{
    auto v = Some_vehicle::Create(10);
    CHECK_EQUAL(10, v->some_prop);

    /* Implicit cast to base pointer works. */
    Vehicle::Ptr ptr = v;
    CHECK(ptr == v);
}
