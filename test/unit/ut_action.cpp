// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <ugcs/vsm/actions.h>

using namespace ugcs::vsm;

Geodetic_tuple geo_pos(1,2,3);

Wgs84_position position(geo_pos);

TEST(convertions_from_base_class_wait)
{
    Action::Ptr action = Wait_action::Create(42);
    CHECK(Action::Type::WAIT == action->Get_type());
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK_EQUAL(42, wa->wait_time);
    Landing_action::Ptr la = action->Get_action<Action::Type::LANDING>();
    CHECK(!la);
}

TEST(convertions_from_base_class_landing)
{
    Action::Ptr action = Landing_action::Create(position, 10, 13.5, 0.42, 2);
    CHECK(Action::Type::LANDING == action->Get_type());
    Landing_action::Ptr la = action->Get_action<Action::Type::LANDING>();
    CHECK_EQUAL(10, la->heading);
    CHECK_EQUAL(13.5, la->elevation);
    CHECK_EQUAL(0.42, la->descend_rate);
    CHECK_EQUAL(2, la->acceptance_radius);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_move)
{
    Action::Ptr action = Move_action::Create(position, 1, 2, 3, 4, 5);
    CHECK(Action::Type::MOVE == action->Get_type());
    Move_action::Ptr ma = action->Get_action<Action::Type::MOVE>();
    CHECK_EQUAL(1, ma->wait_time);
    CHECK_EQUAL(2, ma->acceptance_radius);
    CHECK_EQUAL(3, ma->loiter_orbit);
    CHECK_EQUAL(4, ma->heading);
    CHECK_EQUAL(5, ma->elevation);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_payload_steering)
{
    Action::Ptr action = Payload_steering_action::Create();
    CHECK(Action::Type::PAYLOAD_STEERING == action->Get_type());
    Payload_steering_action::Ptr pa = action->Get_action<Action::Type::PAYLOAD_STEERING>();
    CHECK(pa);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_takeoff)
{
    Action::Ptr action = Takeoff_action::Create(position, 42, 13.5, 0.12, 2);
    CHECK(Action::Type::TAKEOFF == action->Get_type());
    Takeoff_action::Ptr ta = action->Get_action<Action::Type::TAKEOFF>();
    CHECK_EQUAL(42, ta->heading);
    CHECK_EQUAL(13.5, ta->elevation);
    CHECK_EQUAL(0.12, ta->climb_rate);
    CHECK_EQUAL(2, ta->acceptance_radius);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_change_speed)
{
    Action::Ptr action = Change_speed_action::Create(42, 0);
    CHECK(Action::Type::CHANGE_SPEED == action->Get_type());
    Change_speed_action::Ptr ca = action->Get_action<Action::Type::CHANGE_SPEED>();
    CHECK_EQUAL(42, ca->speed);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_set_home)
{
    Action::Ptr action = Set_home_action::Create(true, position, 13.5);
    CHECK(Action::Type::SET_HOME == action->Get_type());
    Set_home_action::Ptr sa = action->Get_action<Action::Type::SET_HOME>();
    CHECK_EQUAL(true, sa->use_current_position);
    CHECK_EQUAL(13.5, sa->elevation);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_poi)
{
    Action::Ptr action = Poi_action::Create(position, true);
    CHECK(Action::Type::POI == action->Get_type());
    Poi_action::Ptr pa = action->Get_action<Action::Type::POI>();
    CHECK(pa->active);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_heading)
{
    Action::Ptr action = Heading_action::Create(M_PI);
    CHECK(Action::Type::HEADING == action->Get_type());
    Heading_action::Ptr ha = action->Get_action<Action::Type::HEADING>();
    CHECK_EQUAL(M_PI, ha->heading);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_camera_control)
{
    Action::Ptr action = Camera_control_action::Create(0, 0, 0, 42);
    CHECK(Action::Type::CAMERA_CONTROL == action->Get_type());
    Camera_control_action::Ptr cc = action->Get_action<Action::Type::CAMERA_CONTROL>();
    CHECK_EQUAL(42, cc->zoom);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_camera_trigger)
{
    Action::Ptr action = Camera_trigger_action::Create(
        proto::CAMERA_MISSION_TRIGGER_STATE_OFF, std::chrono::seconds(1));
    CHECK(Action::Type::CAMERA_TRIGGER == action->Get_type());
    Camera_trigger_action::Ptr ct = action->Get_action<Action::Type::CAMERA_TRIGGER>();
    CHECK_EQUAL(proto::CAMERA_MISSION_TRIGGER_STATE_OFF, ct->state);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_task_attributes)
{
    using Emerg = Task_attributes_action::Emergency_action;

    Action::Ptr action = Task_attributes_action::Create(
            42, Emerg::GO_HOME, Emerg::LAND, Emerg::WAIT);
    CHECK(Action::Type::TASK_ATTRIBUTES == action->Get_type());
    Task_attributes_action::Ptr ta = action->Get_action<Action::Type::TASK_ATTRIBUTES>();
    CHECK_EQUAL(42, ta->safe_altitude);
    CHECK_EQUAL(Emerg::GO_HOME, ta->rc_loss);
    CHECK_EQUAL(Emerg::LAND, ta->gnss_loss);
    CHECK_EQUAL(Emerg::WAIT, ta->low_battery);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

TEST(convertions_from_base_class_panorama)
{
    Action::Ptr action = Panorama_action::Create(
            proto::PANORAMA_MODE_PHOTO,
            M_PI,
            M_PI / 10,
            std::chrono::milliseconds(500),
            M_PI / 10);
    CHECK(Action::Type::PANORAMA == action->Get_type());
    Panorama_action::Ptr pa = action->Get_action<Action::Type::PANORAMA>();
    CHECK_EQUAL(proto::PANORAMA_MODE_PHOTO, pa->trigger_state);
    CHECK(std::chrono::milliseconds(500) == pa->delay);
    Wait_action::Ptr wa = action->Get_action<Action::Type::WAIT>();
    CHECK(!wa);
}

const double tol = 0.000001;

template<class Pld_mission_item_ex>
void Fill_mavlink_position(Pld_mission_item_ex& item)
{
    item->x = 89.999; /* lat grad */
    item->y = 179.999; /* lon grad */
    item->z = 42; /* alt m */
}

#define CHECK_GEO_POSITION(geo_pos) \
    CHECK_CLOSE(1, geo_pos.latitude, tol); \
    CHECK_CLOSE(2, geo_pos.longitude, tol); \
    CHECK_EQUAL(3, geo_pos.altitude);

#define STRINGIFY_(x_) #x_
#define STRINGIFY(x_) STRINGIFY_(x_)

TEST(construct_move)
{
#define P(n,v) p.emplace(n, Property::Create(n, v, proto::FIELD_SEMANTIC_NUMERIC));
    Property_list p;
    P("latitude", 1)
    P("longitude", 2)
    P("altitude_amsl", 3)
    P("acceptance_radius", 3)
    P("heading", 1)
    P("loiter_radius", 5)
    P("wait_time", 1)
    P("ground_elevation", 1.5)
    P("turn_type", 1)

    Move_action ma(p);
    CHECK_GEO_POSITION(ma.position.Get_geodetic());
    CHECK_CLOSE(1, ma.wait_time, tol);
    CHECK_EQUAL(3, ma.acceptance_radius);
    CHECK_EQUAL(5, ma.loiter_orbit);
    CHECK_CLOSE(1, ma.heading, tol);
    CHECK_CLOSE(1.5, ma.elevation, tol);
}


