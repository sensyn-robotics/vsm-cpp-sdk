// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Test for vehicle class.
 */

#include <vsm/vehicle.h>

#include <UnitTest++.h>

using namespace vsm;

class Some_vehicle: public Vehicle
{
    DEFINE_COMMON_CLASS(Some_vehicle, Vehicle)
public:
    int some_prop;

    Some_vehicle(int some_prop):
        Vehicle(mavlink::MAV_TYPE::MAV_TYPE_QUADROTOR,
                mavlink::MAV_AUTOPILOT::MAV_AUTOPILOT_ARDUPILOTMEGA,
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

namespace vsm {
/* Naming to fool the Vehicle to access private system_id field. */
class Ucs_transaction: public vsm::Vehicle
{
    DEFINE_COMMON_CLASS(Ucs_transaction, vsm::Vehicle)
    friend class vsm::Vehicle;
public:
    Ucs_transaction():
        Vehicle(mavlink::MAV_TYPE::MAV_TYPE_QUADROTOR,
                mavlink::MAV_AUTOPILOT::MAV_AUTOPILOT_ARDUPILOTMEGA,
                "ABCD", "1234567890")
    {

    }

    bool Check_id()
    {
        Calculate_system_id();
        return system_id == 252;
    }
};

} /* namespace vsm; */

TEST(id_generator_hash_function)
{
    auto v = Ucs_transaction::Create();
    CHECK(v->Check_id());
}

