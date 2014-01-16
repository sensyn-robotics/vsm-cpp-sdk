// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <vsm/actions.h>

using namespace vsm;

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
    Action::Ptr action = Change_speed_action::Create(42);
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
            Camera_trigger_action::State::OFF, std::chrono::seconds(1));
    CHECK(Action::Type::CAMERA_TRIGGER == action->Get_type());
    Camera_trigger_action::Ptr ct = action->Get_action<Action::Type::CAMERA_TRIGGER>();
    CHECK_EQUAL(Camera_trigger_action::State::OFF, ct->state);
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
            Panorama_action::Trigger_state::ON,
            M_PI,
            M_PI / 10,
            std::chrono::milliseconds(500),
            M_PI / 10);
    CHECK(Action::Type::PANORAMA == action->Get_type());
    Panorama_action::Ptr pa = action->Get_action<Action::Type::PANORAMA>();
    CHECK_EQUAL(Panorama_action::Trigger_state::ON, pa->trigger_state);
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
    CHECK_CLOSE((89.999 * M_PI) / 180.0, geo_pos.latitude, tol); \
    CHECK_CLOSE((179.999 * M_PI) / 180.0, geo_pos.longitude, tol); \
    CHECK_EQUAL(42, geo_pos.altitude);

TEST(construct_from_mavlink_move)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_NAV_WAYPOINT;
    Fill_mavlink_position(item);
    item->param1 = 10; /* wait 1 sec */
    item->param2 = 3; /* acceptance radius 3m */
    item->param3 = 5; /* loiter orbit 5m */
    item->param4 = 45; /* heading grad */

    Move_action ma(item);
    CHECK_GEO_POSITION(ma.position.Get_geodetic());
    CHECK_CLOSE(1, ma.wait_time, tol);
    CHECK_EQUAL(3, ma.acceptance_radius);
    CHECK_EQUAL(5, ma.loiter_orbit);
    CHECK_CLOSE(((45 * M_PI) / 180.0), ma.heading, tol);
}

TEST(construct_from_mavlink_wait)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_CONDITION_DELAY;
    item->param1 = 420; /* 42 sec */

    Wait_action wa(item);
    CHECK_CLOSE(42, wa.wait_time, tol);
}

TEST(construct_from_mavlink_takeoff)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_TAKEOFF_EX;
    Fill_mavlink_position(item);
    item->param2 = 2; /* meters. */
    item->param4 = -89; /* grad */
    item->param3 = 0.42; /* m/s */

    Takeoff_action ta(item);
    CHECK_GEO_POSITION(ta.position.Get_geodetic());
    CHECK_CLOSE(2, ta.acceptance_radius, tol);
    CHECK_CLOSE((-89.0 * M_PI) / 180.0, ta.heading, tol);
    CHECK_CLOSE(0.42, ta.climb_rate, tol);
}

TEST(construct_from_mavlink_landing)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_NAV_LAND_EX;
    Fill_mavlink_position(item);
    item->param2 = 2; /* meters */
    item->param4 = 0; /* grad */
    item->param3 = 0.42; /* m/s */

    Landing_action la(item);
    CHECK_GEO_POSITION(la.position.Get_geodetic());
    CHECK_CLOSE((0.0 * M_PI) / 180.0, la.heading, tol);
    CHECK_CLOSE(0.42, la.descend_rate, tol);
    CHECK_CLOSE(2, la.acceptance_radius, tol);
}

TEST(construct_from_mavlink_change_speed)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_DO_CHANGE_SPEED;
    item->param2 = 42; /* ms */

    Change_speed_action ca(item);
    CHECK_EQUAL(42, ca.speed);
}

TEST(construct_from_mavlink_set_home)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_DO_SET_HOME;
    Fill_mavlink_position(item);
    item->param1 = 0; /* use specified location */

    Set_home_action sa(item);
    CHECK_GEO_POSITION(sa.home_position.Get_geodetic());
    CHECK_EQUAL(false, sa.use_current_position);

    item->param1 = 1; /* use current location */
    Set_home_action sa2(item);
    CHECK_EQUAL(true, sa2.use_current_position);
}

TEST(construct_from_mavlink_poi)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_NAV_ROI;
    Fill_mavlink_position(item);
    item->param1 = mavlink::MAV_ROI::MAV_ROI_LOCATION;

    {
        Poi_action pa(item);
        CHECK_GEO_POSITION(pa.position.Get_geodetic());
        CHECK(pa.active);
    }

    item->param1 = mavlink::MAV_ROI::MAV_ROI_NONE;

    {
        Poi_action pa(item);
        CHECK_GEO_POSITION(pa.position.Get_geodetic());
        CHECK(!pa.active);
    }

    item->param1 = mavlink::MAV_ROI::MAV_ROI_WPINDEX;
    CHECK_THROW(Poi_action pa(item), Action::Format_exception);
}

TEST(construct_from_mavlink_heading)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::MAV_CMD::MAV_CMD_CONDITION_YAW;
    item->param1 = 180; /* deg */
    item->param4 = 0; /* absolute angle. */

    Heading_action ha(item);
    CHECK_CLOSE(M_PI, ha.heading, tol); /* In radians. */

    item->param4 = 1; /* relative angle. */
    CHECK_THROW(Heading_action ha(item), Action::Format_exception);
}

TEST(construct_from_mavlink_camera_control)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_CONTROL;
    item->param1 = 0; /* deg, tilt */
    item->param2 = 90; /* deg, roll */
    item->param3 = -90; /* deg, yaw */
    item->param4 = 1; /* zoom. */

    Camera_control_action cc(item);
    CHECK_EQUAL(0, cc.tilt);
    CHECK_CLOSE(M_PI/2, cc.roll, tol); /* In radians. */
    CHECK_CLOSE(-M_PI/2, cc.yaw, tol); /* In radians. */
    CHECK_EQUAL(1, cc.zoom);
}

TEST(construct_from_mavlink_camera_trigger)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_TRIGGER;
    item->param1 = mavlink::ugcs::MAV_CAMERA_TRIGGER_STATE::CAMERA_TRIGGER_STATE_OFF;
    item->param2 = 420; /* ms */

    Camera_trigger_action ct(item);
    CHECK_EQUAL(Camera_trigger_action::State::OFF, ct.state);
    CHECK_EQUAL(420, ct.interval.count());

    item->param1 = 666666; /* Some unknown value */

    CHECK_THROW(Camera_trigger_action ct(item), Action::Format_exception);
}

TEST(construct_from_mavlink_task_attributes)
{
    using Emerg = mavlink::ugcs::MAV_EMERGENCY_ACTION;

    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_DO_SET_ROUTE_ATTRIBUTES;
    item->param1 = 42;
    item->param2 = Emerg::EMERGENCY_ACTION_GO_HOME;
    item->param3 = Emerg::EMERGENCY_ACTION_LAND;
    item->param4 = Emerg::EMERGENCY_ACTION_WAIT;

    Task_attributes_action ta(item);
    CHECK_EQUAL(42, ta.safe_altitude);
    CHECK_EQUAL(Task_attributes_action::Emergency_action::GO_HOME, ta.rc_loss);
    CHECK_EQUAL(Task_attributes_action::Emergency_action::LAND, ta.gnss_loss);
    CHECK_EQUAL(Task_attributes_action::Emergency_action::WAIT, ta.low_battery);

    item->param2 = 666666; /* Some unknown value */

    CHECK_THROW(Task_attributes_action ta(item), Action::Format_exception);
}

TEST(construct_from_mavlink_panorama)
{
    mavlink::ugcs::Pld_mission_item_ex item;
    item->command = mavlink::ugcs::MAV_CMD::MAV_CMD_DO_PANORAMA;
    item->param1 = mavlink::ugcs::MAV_CAMERA_TRIGGER_STATE::CAMERA_TRIGGER_STATE_ON;
    item->param2 = 180; /* Angle. */
    item->param3 = 180 / 10.0; /* Step. */
    item->param4 = 500; /* Delay. */
    item->x = 180 / 20.0; /* Rotation speed. */

    Panorama_action pa(item);
    CHECK_EQUAL(pa.trigger_state, Panorama_action::Trigger_state::ON);
    CHECK_CLOSE(M_PI, pa.angle, tol);
    CHECK_CLOSE(M_PI / 10, pa.step, tol);
    CHECK(pa.delay == std::chrono::milliseconds(500));
    CHECK_CLOSE(M_PI / 20, pa.speed, tol);

    item->param1 = 666666; /* Some unknown value. */
    CHECK_THROW(Panorama_action pa(item), Action::Format_exception);
}
