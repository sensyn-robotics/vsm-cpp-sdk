// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/task.h>
#include <ugcs/vsm/move_action.h>
#include <ugcs/vsm/set_home_action.h>
#include <ugcs/vsm/takeoff_action.h>

using namespace ugcs::vsm;

Wgs84_position
Task::Get_home_position() const
{
    /* Return only position. */
    return std::get<0>(Get_home_position_impl());
}

double
Task::Get_home_position_altitude() const
{
    return std::get<0>(Get_home_position_impl()).Get_geodetic().altitude;
}

double
Task::Get_takeoff_altitude() const
{
    if (!takeoff_altitude) {
        VSM_EXCEPTION(Internal_error_exception, "Takeoff altitude not set in task.");
    }
    return *takeoff_altitude;
}

void
Task::Set_takeoff_altitude(double altitude)
{
    takeoff_altitude = altitude;
}

double
Task::Get_takeoff_altitude_above_ground() const
{
    return takeoff_altitude_above_ground;
}

void
Task::Set_takeoff_altitude_above_ground(double altitude)
{
    takeoff_altitude_above_ground = altitude;
}

std::tuple<Wgs84_position, double>
Task::Get_home_position_impl() const
{
    Wgs84_position *pos = nullptr;
    double elevation;
    bool first_move_found = false;

    for (auto& action : actions) {
        // Looks like that SET_HOME is obsolete on Server since 3.4
        if (action->Get_type() == Action::Type::SET_HOME) {
            Set_home_action::Ptr a = action->Get_action<Action::Type::SET_HOME>();
            pos = &a->home_position;
            elevation = a->elevation;
            LOG_INFO("Set home from Mission Set Home command. Elevation = %f ", elevation);
            break;
        }
        if ((action->Get_type() == Action::Type::TAKEOFF)  && !first_move_found) {
            Takeoff_action::Ptr a = action->Get_action<Action::Type::TAKEOFF>();
            pos = &a->position;
            elevation = a->elevation;
            LOG_INFO("Set home from mission Takeoff command. Elevation = %f ", elevation);
            first_move_found = true;
        }
        if ((action->Get_type() == Action::Type::MOVE)  && !first_move_found) {
            Move_action::Ptr a = action->Get_action<Action::Type::MOVE>();
            pos = &a->position;
            elevation = a->elevation;
            LOG_INFO("Set home from mission first Move command (takeoff cmd is missing). Elevation = %f ", elevation);
            first_move_found = true;
        }
    }
    if (pos) {
        return std::make_tuple(*pos, elevation);
    }
    VSM_EXCEPTION(ugcs::vsm::Invalid_param_exception, "No home position in the task");
}
