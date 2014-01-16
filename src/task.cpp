// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/task.h>
#include <vsm/move_action.h>
#include <vsm/set_home_action.h>

using namespace vsm;

Wgs84_position
Task::Get_home_position()
{
    /* Return only position. */
    return std::get<0>(Get_home_position_impl());
}

Wgs84_position
Task::Get_home_position_relative()
{
    auto home = Get_home_position_impl();
    Geodetic_tuple gt(std::get<0>(home).Get_geodetic());
    gt.altitude -= std::get<1>(home);
    return Wgs84_position(gt);
}

double
Task::Get_home_position_elevation()
{
    return std::get<1>(Get_home_position_impl());
}

std::tuple<Wgs84_position, double>
Task::Get_home_position_impl()
{
    Wgs84_position *pos = nullptr;
    double elevation;
    bool first_move_found = false;

    for (auto& action: actions) {
        if (action->Get_type() == Action::Type::SET_HOME) {
            Set_home_action::Ptr a = action->Get_action<Action::Type::SET_HOME>();
            pos = &a->home_position;
            elevation = a->elevation;
            break;
        }
        if (action->Get_type() == Action::Type::MOVE && !first_move_found) {
            Move_action::Ptr a = action->Get_action<Action::Type::MOVE>();
            pos = &a->position;
            elevation = a->elevation;
            first_move_found = true;
        }
    }
    if (pos) {
        return std::make_tuple(*pos, elevation);
    }
    VSM_EXCEPTION(vsm::Invalid_param_exception, "No home position in the task");
}
