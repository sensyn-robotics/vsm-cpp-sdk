// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file action.h
 *
 * Generic action. Represents some activity, that can be designed by the
 * system user and afterwards uploaded and executed by the vehicle.
 */
#ifndef _ACTION_H_
#define _ACTION_H_

#include <ugcs/vsm/utils.h>
#include <ugcs/vsm/coordinates.h>
#include <ugcs/vsm/property.h>

namespace ugcs {
namespace vsm {

/** Generic action. Specific actions are determined by @ref Get_type method. */
class Action: public std::enable_shared_from_this<Action> {
    DEFINE_COMMON_CLASS(Action, Action)
public:

    /** Thrown when internal action representation is in a wrong format. */
    VSM_DEFINE_EXCEPTION(Format_exception);

    /** Types of vehicle actions as part of task (mission). */
    enum class Type {
        /** Move action ugcs::vsm::Move_action. */
        MOVE,
        /** Wait action ugcs::vsm::Wait_action. */
        WAIT,
        /** Payload steering action ugcs::vsm::Payload_steering_action. */
        PAYLOAD_STEERING,
        /** Takeoff action ugcs::vsm::Takeoff_action. */
        TAKEOFF,
        /** Landing action ugcs::vsm::Landing_action. */
        LANDING,
        /** Change speed action ugcs::vsm::Change_speed_action. */
        CHANGE_SPEED,
        /** Set home action ugcs::vsm::Set_home_action. */
        SET_HOME,
        /** Point of interest ugcs::vsm::Poi_action. */
        POI,
        /** Heading ugcs::vsm::Heading_action. */
        HEADING,
        /** Camera control action ugcs::vsm::Camera_control_action. */
        CAMERA_CONTROL,
        /** Camera trigger action ugcs::vsm::Camera_trigger_action. */
        CAMERA_TRIGGER,
        /** Panorama action ugcs::vsm::Panorama_action. */
        PANORAMA,
        /** Task attributes action ugcs::vsm::Task_attributes_action. */
        TASK_ATTRIBUTES,
        /** Continuously shot camera in regular time intervals. */
        CAMERA_SERIES_BY_TIME,
        /** Continuously shot camera in regular distance intervals. */
        CAMERA_SERIES_BY_DISTANCE,
        /** Set mission parameter. see enum MAV_MISSION_PARAMETER_TYPE */
        SET_PARAMETER,
        /** Set servo PWM*/
        SET_SERVO,
        /** Repeat servo movement between given PWM and 1500 */
        REPEAT_SERVO,
    };

    /** Construct action of specific type. */
    Action(Type type) : type(type) {}

    /** Make sure Action is polymorphic. */
    virtual ~Action() {}

    /** Get action type. */
    Type
    Get_type() const
    {
        return type;
    }

    void
    Set_id(int id) {
        command_id = id;
    }

    /** Get human readable name of the action. */
    std::string
    Get_name()
    {
        switch (type) {
        case Type::MOVE: return "MOVE";
        case Type::WAIT: return "WAIT";
        case Type::PAYLOAD_STEERING: return "PAYLOAD STEERING";
        case Type::TAKEOFF: return "TAKEOFF";
        case Type::LANDING: return "LANDING";
        case Type::CHANGE_SPEED: return "CHANGE SPEED";
        case Type::SET_HOME: return "SET HOME";
        case Type::POI: return "POI";
        case Type::HEADING: return "HEADING";
        case Type::CAMERA_CONTROL: return "CAMERA CONTROL";
        case Type::CAMERA_TRIGGER: return "CAMERA TRIGGER";
        case Type::PANORAMA: return "PANORAMA";
        case Type::TASK_ATTRIBUTES: return "TASK ATTRIBUTES";
        case Type::CAMERA_SERIES_BY_TIME: return "CAMERA SERIES BY TIME";
        case Type::CAMERA_SERIES_BY_DISTANCE: return "CAMERA SERIES BY DISTANCE";
        case Type::SET_PARAMETER: return "SET_PARAMETER";
        case Type::SET_SERVO: return "SET_SERVO";
        case Type::REPEAT_SERVO: return "REPEAT_SERVO";
        }
        VSM_EXCEPTION(Internal_error_exception, "Action type %d unknown.",
                      static_cast<int>(type));
    }

    /** Map Action type enum value to specific Action type class. */
    template <Type type_val>
    struct Mapper {
        /** By default, maps to Action itself. */
        typedef Action type;
    };

    /** Get pointer to specific action as determined by @ref Get_type.
     *
     * @return Non-null shared pointer to specific action if type value given
     * in template parameter is equal with the type returned by @ref Get_type,
     * otherwise empty shared pointer is returned.
     */
    template<Type type_val>
    typename Mapper<type_val>::type::Ptr Get_action()
    {
        if (type_val == type) {
            return std::static_pointer_cast<typename Mapper<type_val>::type>(Shared_from_this());
        } else {
            return nullptr;
        }
    }

    int command_id = -1;

private:

    /** Type of the action. */
    const Type type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _ACTION_H_ */
