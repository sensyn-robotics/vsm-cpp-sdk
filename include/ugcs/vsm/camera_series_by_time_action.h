// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file camera_control_action.h
 */
#ifndef _CAMERA_SERIES_BY_TIME_ACTION_H_
#define _CAMERA_SERIES_BY_TIME_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Performs a series of consequent camera shots in a fixed time intervals. */
class Camera_series_by_time_action: public Action {
    DEFINE_COMMON_CLASS(Camera_series_by_time_action, Action)

public:

    Camera_series_by_time_action(std::chrono::milliseconds interval,
                                 Optional<int> count,
                                 std::chrono::milliseconds initial_delay):
        Action(Type::CAMERA_SERIES_BY_TIME),
        interval(interval),
        count(count),
        initial_delay(initial_delay)
    {}

    /**
     * Construct camera control action from Mavlink mission item.
     *
     * @param item With command equal to mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_SERIES_BY_TIME
     */
    Camera_series_by_time_action(const mavlink::ugcs::Pld_mission_item_ex& item):
        Action(Type::CAMERA_SERIES_BY_TIME),
        interval(std::chrono::milliseconds(item->param1)),
        count(item->param2 == UINT32_MAX ? Optional<int>() : Optional<int>(item->param2)),
        initial_delay(std::chrono::milliseconds(item->param3))
    {
        ASSERT(item->command == mavlink::ugcs::MAV_CMD::MAV_CMD_DO_CAMERA_SERIES_BY_TIME);
    }

    /**
     * Construct action from protobuf command.
     */
    Camera_series_by_time_action(const Property_list& p) :
        Action(Type::CAMERA_SERIES_BY_TIME)
    {
        int tmp;
        float time;

        if (p.at("count")->Get_value(tmp)) {
            count = tmp;
        }
        p.at("period")->Get_value(time);
        tmp = time * 1000;
        interval = std::chrono::milliseconds(tmp);

        p.at("delay")->Get_value(time);
        tmp = time * 1000;
        initial_delay = std::chrono::milliseconds(tmp);
    }

    /** Time interval between two consequent shots. */
    std::chrono::milliseconds interval;
    /** Total number of shots to perform. */
    Optional<int> count;
    /** Initial delay. Time interval between the command has been accepted by
     * the system and shooting the first picture.
     */
    std::chrono::milliseconds initial_delay;
};

/** Type mapper for camera control action. */
template<>
struct Action::Mapper<Action::Type::CAMERA_SERIES_BY_TIME> {
    /** Real type. */
    typedef Camera_series_by_time_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _CAMERA_SERIES_BY_TIME_ACTION_H_ */
